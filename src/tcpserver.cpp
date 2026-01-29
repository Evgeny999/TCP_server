#include "tcpserver.h"

#include <iostream>
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

#include "logger.h"


// Инициализация статического указателя на экземпляр
TcpServer* TcpServer::s_instance = nullptr;

TcpServer::TcpServer(int port,
                     const std::string& logFilePath)
    : m_loop(nullptr),
      m_tcpPort(port),
      m_logFilePath(logFilePath),
      m_tcpAcceptWatcher(nullptr),
      m_tcpFd(-1),
      m_running(false) {
  // Инициализируем логгер
  m_logger = std::make_unique<Logger>(logFilePath);

  // Сохраняем указатель на экземпляр для статических callback-функций
  s_instance = this;
}

TcpServer::~TcpServer() {  // Останавливаем сервер если еще работает
  if (m_running) {
    stop();
  }

  // Обнуляем статический указатель
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

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
    std::cerr << "Error in read callback" << std::endl;
    return;
  }

  Client* client = static_cast<Client*>(w->data);
  if (!client || !s_instance) return;

  // Буфер для чтения данных
  char buffer[4096];
  ssize_t bytesRead = recv(client->fd, buffer, sizeof(buffer), 0);

  if (bytesRead > 0) {
    // Логируем полученные данные
    s_instance->logClientData(client->fd, buffer, bytesRead);

    // Сохраняем данные в буфер клиента для отправки обратно
    client->buffer.insert(client->buffer.end(), buffer, buffer + bytesRead);

    // Если есть данные для отправки, активируем watcher для записи
    if (!client->buffer.empty()) {
      ev_io_stop(loop, client->writeWatcher);
      ev_io_set(client->writeWatcher, client->fd, EV_WRITE);
      ev_io_start(loop, client->writeWatcher);
    }

    std::cout << "Received " << bytesRead << " bytes from "
              << client->peerAddress << std::endl;
  } else if (bytesRead == 0) {
    // Клиент закрыл соединение
    std::cout << "Connection closed by " << client->peerAddress << std::endl;
    s_instance->closeClient(client->fd);
  } else {
    // Ошибка при чтении
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "Error reading from " << client->peerAddress << ": "
                << strerror(errno) << std::endl;
      s_instance->closeClient(client->fd);
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

void TcpServer::closeClient(int clientFd) {
  auto it = m_clients.find(clientFd);
  if (it == m_clients.end()) return;

  // Останавливаем и удаляем watchers
  if (it->second->readWatcher) {
    ev_io_stop(m_loop, it->second->readWatcher);
    delete it->second->readWatcher;
  }

  if (it->second->writeWatcher) {
    ev_io_stop(m_loop, it->second->writeWatcher);
    delete it->second->writeWatcher;
  }

  // Закрываем файловый дескриптор
  close(clientFd);

  std::cout << "Closed connection from " << it->second->peerAddress
            << " (fd: " << clientFd << ")" << std::endl;

  // Удаляем клиента из карты
  m_clients.erase(it);
}

std::string TcpServer::getPeerAddress(int fd) const {
  struct sockaddr_storage addr;
  socklen_t addrLen = sizeof(addr);

  if (getpeername(fd, (struct sockaddr*)&addr, &addrLen) < 0) {
    return "unknown";
  }

  char host[INET6_ADDRSTRLEN];
  char port[6];

  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* s = (struct sockaddr_in*)&addr;
    inet_ntop(AF_INET, &s->sin_addr, host, sizeof(host));
    snprintf(port, sizeof(port), "%d", ntohs(s->sin_port));
    return std::string(host) + ":" + port;
  } else {
    struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
    inet_ntop(AF_INET6, &s->sin6_addr, host, sizeof(host));
    snprintf(port, sizeof(port), "%d", ntohs(s->sin6_port));
    return std::string(host) + ":" + port;
  }
}

void TcpServer::logClientData(int clientFd, const char* data, size_t size) {
  std::string clientInfo = getPeerAddress(clientFd);
  if (m_logger) {
    m_logger->log(clientInfo, data, size);
  }
}

void TcpServer::run() {
  // Инициализируем основной цикл событий
  m_loop = ev_loop_new(EVFLAG_AUTO);
  if (!m_loop) {
    std::cerr << "Error creating event loop" << std::endl;
    return;
  }

  // Инициализируем TCP сервер
  if (m_tcpPort > 0) {
    if (!initTcpServer()) {
      std::cerr << "Failed to initialize TCP server" << std::endl;
      return;
    }

    // Создаем watcher для принятия TCP подключений
    m_tcpAcceptWatcher = new ev_io();
    ev_io_init(m_tcpAcceptWatcher, acceptCallback, m_tcpFd, EV_READ);
    ev_io_start(m_loop, m_tcpAcceptWatcher);
  }

  std::cout << "Tcp server started" << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;

  m_running = true;

  // Запускаем основной цикл событий
  ev_run(m_loop, 0);
}

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

  // Уничтожаем логгер
  m_logger.reset();

  std::cout << "Server stopped" << std::endl;
}
