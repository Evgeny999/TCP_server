#!/bin/bash

# test_client.sh
# Скрипт для тестирования сервера с 20 одновременными подключениями

SERVER_HOST="localhost"
SERVER_PORT=5000
NUM_CONNECTIONS=20

echo "Starting test with $NUM_CONNECTIONS connections..."

# Функция для одного подключения
connect_and_send() {
    local client_id=$1
    MESSAGE="Hello from client $((RANDOM % 10000)) at $(date '+%H:%M:%S')"
    local unique_msg="$MESSAGE - Client $client_id"
    
    # Используем netcat для подключения и отправки данных
    # -q 0: закрыть соединение после отправки
    # -w 5: таймаут 5 секунд
    echo -e "$unique_msg\n" | nc -q 0 -w 5 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null
    
    if [ $? -eq 0 ]; then
        echo "Client $client_id: Sent and received response"
    else
        echo "Client $client_id: Connection failed"
    fi
}

# Запускаем подключения параллельно
for i in $(seq 1 $NUM_CONNECTIONS); do
    connect_and_send $i &
done

# Ждем завершения всех фоновых процессов
wait

echo "Test completed"
