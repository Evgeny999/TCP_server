#include <iomanip>
#include <iostream>
#include "logger.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <system_error>
#include "logger.h"
#include "tcpserver.h"

// Инициализация статического указателя на экземпляр
TcpServer* TcpServer::s_instance = nullptr;

TcpServer::TcpServer(int port, const std::string& unixSocketPath,
                     const std::string& logFilePath)
    : m_loop(nullptr),
      m_tcpPort(port),
      m_unixSocketPath(unixSocketPath),
      m_logFilePath(logFilePath),
      m_tcpAcceptWatcher(nullptr),
      m_unixAcceptWatcher(nullptr),
      m_tcpFd(-1),
      m_running(false) {
  // Сохраняем указатель на экземпляр для статических callback-функций
  s_instance = this;
}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::initTcpServer() {
  // Создаем TCP сокет
  m_tcpFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (m_tcpFd < 0) {
    std::cerr << "Error creating TCP socket: " << strerror(errno) << std::endl;
    return false;
  }

  // Устанавливаем опцию REUSEADDR для быстрого переиспользования порта
  int opt = 1;
  if (setsockopt(m_tcpFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "Error setting SO_REUSEADDR: " << strerror(errno) << std::endl;
    close(m_tcpFd);
    return false;
  }

  // Настраиваем адрес сервера
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(m_tcpPort);

  // Привязываем сокет к адресу
  if (bind(m_tcpFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "Error binding TCP socket: " << strerror(errno) << std::endl;
    close(m_tcpFd);
    return false;
  }

  // Начинаем прослушивание
  if (listen(m_tcpFd, SOMAXCONN) < 0) {
    std::cerr << "Error listening on TCP socket: " << strerror(errno)
              << std::endl;
    close(m_tcpFd);
    return false;
  }

  std::cout << "TCP server listening on port " << m_tcpPort << std::endl;
  return true;
}

void TcpServer::acceptCallback(struct ev_loop* loop, ev_io* w, int revents) {
  if (!s_instance) return;

  if (revents & EV_ERROR) {
    std::cerr << "Error in accept callback" << std::endl;
    return;
  }

  // Принимаем новое подключение
  struct sockaddr_in clientAddr;
  socklen_t clientLen = sizeof(clientAddr);
  int clientFd = accept(w->fd, (struct sockaddr*)&clientAddr, &clientLen);

  if (clientFd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "Error accepting connection: " << strerror(errno)
                << std::endl;
    }
    return;
  }

  // Устанавливаем неблокирующий режим
  int flags = fcntl(clientFd, F_GETFL, 0);
  fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

  // Создаем структуру для клиента
  auto client = std::make_unique<Client>();
  client->fd = clientFd;
  client->peerAddress = s_instance->getPeerAddress(clientFd);

  // Создаем watcher для чтения
  client->readWatcher = new ev_io();
  ev_io_init(client->readWatcher, readCallback, clientFd, EV_READ);
  client->readWatcher->data = client.get();
  ev_io_start(loop, client->readWatcher);

  // Создаем watcher для записи (пока не активируем)
  client->writeWatcher = new ev_io();
  ev_io_init(client->writeWatcher, writeCallback, clientFd, EV_WRITE);
  client->writeWatcher->data = client.get();

  // Сохраняем клиента
  s_instance->m_clients[clientFd] = std::move(client);

  std::cout << "New connection from "
            << s_instance->m_clients[clientFd]->peerAddress
            << " (fd: " << clientFd << ")" << std::endl;
}

void TcpServer::readCallback(struct ev_loop* loop, ev_io* w, int revents) {
  if (revents & EV_ERROR) {
    std::cerr << "Error in write callback" << std::endl;
    return;
  }

  Client* client = static_cast<Client*>(w->data);
  if (!client || client->buffer.empty()) {
    // Нет данных для отправки, останавливаем watcher
    ev_io_stop(loop, w);
    return;
  }

  // Отправляем данные из буфера
  ssize_t bytesSent =
      send(client->fd, client->buffer.data(), client->buffer.size(), 0);

  if (bytesSent > 0) {
    // Удаляем отправленные данные из буфера
    client->buffer.erase(client->buffer.begin(),
                         client->buffer.begin() + bytesSent);

    std::cout << "Sent " << bytesSent << " bytes to " << client->peerAddress
              << std::endl;

    // Если буфер пуст, останавливаем watcher для записи
    if (client->buffer.empty()) {
      ev_io_stop(loop, w);
    }
  } else if (bytesSent < 0) {
    // Ошибка при отправке
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "Error writing to " << client->peerAddress << ": "
                << strerror(errno) << std::endl;
      if (s_instance) {
        s_instance->closeClient(client->fd);
      }
    }
  }
}

void TcpServer::writeCallback(struct ev_loop* loop, ev_io* w, int revents) {
  if (revents & EV_ERROR) {
    std::cerr << "Error in write callback" << std::endl;
    return;
  }

  Client* client = static_cast<Client*>(w->data);
  if (!client || client->buffer.empty()) {
    // Нет данных для отправки, останавливаем watcher
    ev_io_stop(loop, w);
    return;
  }

  // Отправляем данные из буфера
  ssize_t bytesSent =
      send(client->fd, client->buffer.data(), client->buffer.size(), 0);

  if (bytesSent > 0) {
    // Удаляем отправленные данные из буфера
    client->buffer.erase(client->buffer.begin(),
                         client->buffer.begin() + bytesSent);

    std::cout << "Sent " << bytesSent << " bytes to " << client->peerAddress
              << std::endl;

    // Если буфер пуст, останавливаем watcher для записи
    if (client->buffer.empty()) {
      ev_io_stop(loop, w);
    }
  } else if (bytesSent < 0) {
    // Ошибка при отправке
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "Error writing to " << client->peerAddress << ": "
                << strerror(errno) << std::endl;
      if (s_instance) {
        s_instance->closeClient(client->fd);
      }
    }
  }
}

void TcpServer::closeClient(int clientFd) {}

std::string TcpServer::getPeerAddress(int fd) const {}

void TcpServer::logClientData(int clientFd, const char* data, size_t size) {}

void TcpServer::run() {}

void TcpServer::stop() {
  if (!m_running) return;

  m_running = false;

  // Останавливаем основной цикл событий
  if (m_loop) {
    ev_break(m_loop, EVBREAK_ALL);
  }

  // Закрываем все клиентские соединения
  for (auto& pair : m_clients) {
    closeClient(pair.first);
  }
  m_clients.clear();

  // Останавливаем и удаляем accept watchers
  if (m_tcpAcceptWatcher) {
    ev_io_stop(m_loop, m_tcpAcceptWatcher);
    delete m_tcpAcceptWatcher;
    m_tcpAcceptWatcher = nullptr;
  }

  if (m_unixAcceptWatcher) {
    ev_io_stop(m_loop, m_unixAcceptWatcher);
    delete m_unixAcceptWatcher;
    m_unixAcceptWatcher = nullptr;
  }

  // Закрываем файловые дескрипторы серверов
  if (m_tcpFd >= 0) {
    close(m_tcpFd);
    m_tcpFd = -1;
  }

  // Уничтожаем основной цикл событий
  if (m_loop) {
    ev_loop_destroy(m_loop);
    m_loop = nullptr;
  }

  std::cout << "Server stopped" << std::endl;
}
