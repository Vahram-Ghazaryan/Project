cmake_minimum_required(VERSION 3.10)

project(chat_client)

set(CMAKE_CXX_STANDARD 17)

file(GLOB CPPS "src/*.cpp")

include_directories("${PROJECT_BINARY_DIR}")

include_directories("${PROJECT_SOURCE_DIR}/include")

add_executable(${PROJECT_NAME} ${CPPS})
