cmake_minimum_required(VERSION 3.10)
project(TcpServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск библиотеки libev
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(LIBEV REQUIRED libev)

# Настройка путей для libev
include_directories(${LIBEV_INCLUDE_DIRS})
link_directories(${LIBEV_LIBRARY_DIRS})

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/tcpserver.cpp
    src/tcpserver.h
    src/logger.cpp
    src/logger.h
)

# Создание исполняемого файла
add_executable(tcpserver ${SOURCES})

# Линковка с libev
target_link_libraries(tcpserver ${LIBEV_LIBRARIES})
