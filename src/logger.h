#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
/**
 * @brief Класс для потокобезопасного логирования в файл
 */
class Logger {
public:
    /**
     * @brief Конструктор логгера
     * @param filename Имя файла для логирования
     */
    explicit Logger(const std::string& filename);

    /**
     * @brief Деструктор логгера
     */
    ~Logger();

    /**
     * @brief Запись сообщения в лог
     * @param clientInfo Информация о клиенте
     * @param data Данные, полученные от клиенте
     * @param size Размер данных
     */
    void log(const std::string& clientInfo, const char* data, size_t size);

    /**
     * @brief Получение текущей даты и времени в формате строки
     * @return Строка с датой и временем
     */
    static std::string currentDateTime();

private:
    std::ofstream m_logFile;  // Файл для записи логов
    std::mutex m_mutex;       // Мьютекс для потокобезопасности
};

#endif // LOGGER_H
