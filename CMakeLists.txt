cmake_minimum_required(VERSION 3.10)
project(web_client)

set(CMAKE_CXX_STANDARD 17)

add_executable(web_client main.cpp)

target_include_directories(web_client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Crow/include
)
