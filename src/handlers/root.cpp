#include "../handlers.hpp"
#include "common.hpp"

void register_root(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/")
        .methods(
            crow::HTTPMethod::GET,
            crow::HTTPMethod::POST,
            crow::HTTPMethod::PUT,
            crow::HTTPMethod::DELETE,
            crow::HTTPMethod::PATCH,
            crow::HTTPMethod::HEAD,
            crow::HTTPMethod::OPTIONS
        )
        ([&redis](const crow::request& req) {
            return handle_request(req, redis);
        });
}
