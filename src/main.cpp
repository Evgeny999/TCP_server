#include <csignal>
#include <cstdlib>
#include <iostream>
#include "tcpserver.h"

TcpServer* g_server = nullptr;

// Обработчик сигнала для graceful shutdown
void signalHandler(int signal) {
  std::cout << "\nReceived signal " << signal << ", shutting down..."
            << std::endl;
  if (g_server) {
    g_server->stop();
  }
}

int main(int argc, char* argv[]) {
  // Регистрируем обработчики сигналов
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  try {
    // Создаем и запускаем сервер
    // Можно указать параметры: порт, путь к Unix-сокету, файл логов
    std::string unixSocketPath = "";
    std::string logFilePath = "server.log";

    // Пример использования аргументов командной строки
    if (argc > 1) {
      unixSocketPath = argv[1];
    }
    if (argc > 2) {
      logFilePath = argv[2];
    }

    TcpServer server(5000, unixSocketPath, logFilePath);
    g_server = &server;

    server.run();

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Server terminated normally" << std::endl;
  return 0;
}
