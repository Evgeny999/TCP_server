#include "logger.h"
#include <iomanip>
#include <iostream>

#include "tcpserver.h"
#include "logger.h"
#include <ev.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/un.h>
#include <iostream>
#include <system_error>

// Инициализация статического указателя на экземпляр
TcpServer* TcpServer::s_instance = nullptr;

TcpServer::TcpServer(int port,
                       const std::string& unixSocketPath,
                       const std::string& logFilePath)
    : m_loop(nullptr)
    , m_tcpPort(port)
    , m_unixSocketPath(unixSocketPath)
    , m_logFilePath(logFilePath)
    , m_tcpAcceptWatcher(nullptr)
    , m_unixAcceptWatcher(nullptr)
    , m_tcpFd(-1)
    , m_running(false) {

    // Сохраняем указатель на экземпляр для статических callback-функций
    s_instance = this;
}

TcpServer::~TcpServer() {

}


bool TcpServer::initTcpServer() {

}

bool TcpServer::initUnixServer() {

}

void TcpServer::acceptCallback(struct ev_loop* loop, ev_io* w, int revents) {

}

void TcpServer::readCallback(struct ev_loop* loop, ev_io* w, int revents) {

}

void TcpServer::writeCallback(struct ev_loop* loop, ev_io* w, int revents) {

}

void TcpServer::closeClient(int clientFd) {

}

std::string TcpServer::getPeerAddress(int fd) const {

}

void TcpServer::logClientData(int clientFd, const char* data, size_t size) {

}

void TcpServer::run() {

}

void TcpServer::stop() {

}
