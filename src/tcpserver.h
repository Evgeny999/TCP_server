#ifndef ECHOSERVER_H
#define ECHOSERVER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// forward declaration объявление структур libev
struct ev_loop;
struct ev_io;
struct ev_timer;

/**
 * @brief Класс для реализации неблокирующего эхо-сервера на libev
 */
class TcpServer {
public:
    /**
     * @brief Конструктор сервера
     * @param port Порт для прослушивания TCP (0 для отключения)
     * @param unixSocketPath Путь к Unix-сокету (пустая строка для отключения)
     * @param logFilePath Путь к файлу логов
     */
    TcpServer(int port = 5000,
               const std::string& unixSocketPath = "",
               const std::string& logFilePath = "server.log");

    /**
     * @brief Деструктор сервера
     */
    ~TcpServer();

    /**
     * @brief Запуск основного цикла сервера
     */
    void run();

    /**
     * @brief Остановка сервера
     */
    void stop();

private:
    // Структура для хранения информации о клиенте
    struct Client {
        int fd;                              // Файловый дескриптор
        ev_io* readWatcher;                  // Watcher для чтения
        ev_io* writeWatcher;                 // Watcher для записи
    };

    // Основной цикл событий
    ev_loop* m_loop;

    // Параметры сервера
    int m_tcpPort;
    std::string m_unixSocketPath;
    std::string m_logFilePath;

    // Watchers для прослушивания сокетов
    ev_io* m_tcpAcceptWatcher;
    ev_io* m_unixAcceptWatcher;

    // Карта клиентов по файловым дескрипторам
    std::unordered_map<int, std::unique_ptr<Client>> m_clients;

    // Файловый дескриптор для TCP сокета
    int m_tcpFd;

    // Флаг работы сервера
    bool m_running;

    /**
     * @brief Инициализация TCP сервера
     * @return true если успешно, false в случае ошибки
     */
    bool initTcpServer();

    /**
     * @brief Инициализация Unix сокета
     * @return true если успешно, false в случае ошибки
     */
    bool initUnixServer();

    /**
     * @brief Обработчик нового подключения
     * @param w Watcher, который сработал
     * @param revents События
     */
    static void acceptCallback(ev_loop* loop, ev_io* w, int revents);

    /**
     * @brief Обработчик чтения данных от клиента
     * @param w Watcher, который сработал
     * @param revents События
     */
    static void readCallback(ev_loop* loop, ev_io* w, int revents);

    /**
     * @brief Обработчик записи данных клиенту
     * @param w Watcher, который сработал
     * @param revents События
     */
    static void writeCallback(ev_loop* loop, ev_io* w, int revents);

    /**
     * @brief Закрытие соединения с клиентом
     * @param clientFd Файловый дескриптор клиента
     */
    void closeClient(int clientFd);

    /**
     * @brief Получение адреса клиента в читаемом формате
     * @param fd Файловый дескриптор клиента
     * @return Строка с адресом клиента
     */
    std::string getPeerAddress(int fd) const;

    /**
     * @brief Логирование данных клиента
     * @param clientFd Файловый дескриптор клиента
     * @param data Полученные данные
     * @param size Размер данных
     */
    void logClientData(int clientFd, const char* data, size_t size);

    // Экземпляр для доступа к методам класса из статических callback-функций
    static TcpServer* s_instance;
};

#endif // ECHOSERVER_H
