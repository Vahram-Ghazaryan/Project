cmake_minimum_required(VERSION 3.10)
project(chat_client)

set(CMAKE_CXX_STANDARD 17)

# Указываем путь к Boost
set(BOOST_ROOT "/home/aram/Desktop/project1/boost_1_85_0")

# Поиск Boost
find_package(Boost 1.85 REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Указываем все исходные файлы
set(SOURCE_FILES
    src/client.cpp
    src/client_utils.cpp
    src/main.cpp
)

# Создаем исполняемый файл
add_executable(chat_client ${SOURCE_FILES})

# Линкуем с библиотеками Boost
target_link_libraries(chat_client ${Boost_LIBRARIES})

