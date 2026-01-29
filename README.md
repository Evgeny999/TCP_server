# Установка зависимостей
sudo apt update
sudo apt install cmake libev-dev pkg-config

# Сборка проекта
mkdir build
cd build
cmake ..
make

# Запуск сервера
./tcpserver

# В другом терминале запустить тест
chmod +x ../test/test_client.sh
./../test/test_client.sh
