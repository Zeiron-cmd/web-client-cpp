#include "../handlers.hpp"
#include "../session.hpp"
#include "../utils.hpp"

void register_logout(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/logout")
    ([&redis](const crow::request& req) {

        std::string session = extract_session(req.get_header_value("Cookie"));
        if (session.empty()) {
            return crow::response(302).add_header("Location", "/");
        }

        std::string session_key = "session:" + session;
        auto data = redis.get(session_key);
        if (!data) {
            return crow::response(302).add_header("Location", "/");
        }

        auto parsed = parse_session(*data);
        (void)parsed;

        redis.del(session_key);
        return crow::response(302).add_header("Location", "/");
    });
}
