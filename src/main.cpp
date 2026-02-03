#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "tcpserver.h"

#define TCP_PORT 5000

std::unique_ptr<TcpServer> g_server;

void signalHandler(int signal) {
  std::cout << "\nReceived signal " << signal << ", shutting down..."
            << std::endl;

  if (g_server) {
    g_server->stop();
    // Явно уничтожаем сервер перед выходом
    g_server.reset();
  }

  std::cout << "Shutdown complete" << std::endl;

  // Без exit(): ev_run продолжает выполняться после выхода из обработчика, но
  // m_loop уже освобожден. Тогда в ev_run произойдёт попытка обращения к
  // освобождённой памяти и случится segfault
  // С exit(): процесс убивается немедленно и ev_run не продолжает работу после
  // выхода из обработчика
  std::exit(0);
}

int main(int argc, char* argv[]) {
  // Установка обработчиков сигналов
  struct sigaction sa;
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  // Игнорируем SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  try {
    g_server = std::make_unique<TcpServer>(TCP_PORT, "server.log");
    std::cout << "Starting TCP echo server on port " << TCP_PORT << "..."
              << std::endl;
    g_server->run();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Unknown error occurred" << std::endl;
    return 1;
  }

  // Очищаем перед выходом
  if (g_server) {
    g_server.reset();
  }

  return 0;
}
