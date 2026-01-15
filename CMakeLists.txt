cmake_minimum_required(VERSION 3.16)
project(web_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Crow REQUIRED)
find_package(redis++ REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(web-client
    src/main.cpp
    src/http.cpp
    src/redis.cpp
    src/handlers/root.cpp
    src/handlers/login.cpp
    src/handlers/logout.cpp
    src/handlers/common.cpp
    src/api/auth_client.cpp
    src/api/main_client.cpp
)

target_link_libraries(web-client
    Crow::Crow
    redis++::redis++
    nlohmann_json::nlohmann_json
    uuid
    hiredis
    curl
)
