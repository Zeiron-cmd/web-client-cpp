#include "../handlers.hpp"

void register_logout(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/logout")
    ([&redis](const crow::request& req) {

        std::string session = extract_session(req.get_header_value("Cookie"));
        if (!session.empty()) {
            redis.del("session:" + session);
        }

        return crow::response(302).add_header("Location", "/");
    });
}
