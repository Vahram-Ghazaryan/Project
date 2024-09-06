cmake_minimum_required(VERSION 3.10)

project(chat_client)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)

file(GLOB CPPS "src/*.cpp")

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(${PROJECT_NAME} ${CPPS})

target_link_libraries(${PROJECT_NAME} OpenSSL::SSL OpenSSL::Crypto)

