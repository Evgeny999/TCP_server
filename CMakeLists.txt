cmake_minimum_required(VERSION 3.10)
project(TcpServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Сначала пробуем найти через pkg-config
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBEV QUIET libev)
endif()

# Если не нашли через pkg-config, ищем вручную
# TODO (Ubuntu) libev-dev установлен, но почему-то не устанавливается его pc
# файл. тогда pkg_config не может найти libev
if(NOT LIBEV_FOUND)
    find_path(LIBEV_INCLUDE_DIR ev.h
        PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
    )

    find_library(LIBEV_LIBRARY
        NAMES ev libev
        PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    )

    if(LIBEV_INCLUDE_DIR AND LIBEV_LIBRARY)
        set(LIBEV_FOUND TRUE)
        set(LIBEV_INCLUDE_DIRS ${LIBEV_INCLUDE_DIR})
        set(LIBEV_LIBRARIES ${LIBEV_LIBRARY})
    endif()
endif()

if(NOT LIBEV_FOUND)
    message(FATAL_ERROR "libev not found! Install libev-dev package")
endif()

message(STATUS "Found libev: ${LIBEV_LIBRARIES}")
message(STATUS "Include dirs: ${LIBEV_INCLUDE_DIRS}")

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

# Включаем директории с заголовками
target_include_directories(tcpserver PRIVATE ${LIBEV_INCLUDE_DIRS})

# Линковка с libev
target_link_libraries(tcpserver ${LIBEV_LIBRARIES})
