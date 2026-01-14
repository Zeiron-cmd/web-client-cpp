#include "../handlers.hpp"
#include "common.hpp"

void register_root(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/")
    ([&redis](const crow::request& req) {
        return handle_request(req, redis);
    });
}
