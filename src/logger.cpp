#include "logger.h"
#include <iomanip>
#include <iostream>

Logger::Logger(const std::string& filename) {
  m_logFile.open(filename, std::ios::app);
  if (!m_logFile.is_open()) {
    std::cerr << "Error: Could not open log file " << filename << std::endl;
  }
}

Logger::~Logger() {
  if (m_logFile.is_open()) {
    m_logFile.close();
  }
}

void Logger::log(const std::string& clientInfo, const char* data, size_t size) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_logFile.is_open()) {
    return;
  }

  try {
    // Записываем дату, время и информацию о клиенте
    m_logFile << "[" << currentDateTime() << "] "
              << "Client: " << clientInfo << ", Data: \"";

    // Записываем данные, заменяя непечатаемые символы
    for (size_t i = 0; i < size; ++i) {
      if (data[i] > 32 && data[i] <= 126) {
        m_logFile << data[i];
      } else {
        m_logFile << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                  << (int)(unsigned char)data[i];
      }
    }

    m_logFile << "\", Size: " << std::dec << size << " bytes" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Logging error: " << e.what() << std::endl;
  }
}

std::string Logger::currentDateTime() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "."
     << std::setfill('0') << std::setw(3) << ms.count();

  return ss.str();
}
