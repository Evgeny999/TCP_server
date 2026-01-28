#include "tcpserver.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

TcpServer* g_server = nullptr;

int main(int argc, char* argv[]) {

  try {
    // Создаем и запускаем сервер
    // Можно указать параметры: порт, путь к Unix-сокету, файл логов
    std::string unixSocketPath = "";
    std::string logFilePath = "server.log";

    // Пример использования аргументов командной строки
    if (argc > 1) {
      unixSocketPath = argv[1];
    }
    if (argc > 1) {
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
