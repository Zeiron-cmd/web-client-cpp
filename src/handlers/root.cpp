#include "../handlers.hpp"
#include <nlohmann/json.hpp>

void register_root(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/")
    ([&redis](const crow::request& req) {

        std::string session = extract_session(req.get_header_value("Cookie"));

        if (session.empty()) {
            return crow::response(
                "<h1>Login</h1>"
                "<a href='/login?type=github'>GitHub</a><br>"
                "<a href='/login?type=yandex'>Yandex</a><br>"
                "<a href='/login?type=code'>Code</a>"
            );
        }

        auto data = redis.get("session:" + session);
        if (!data) {
            return crow::response(302).add_header("Location", "/");
        }

        auto json = nlohmann::json::parse(*data);

        if (json["status"] == "authorized") {
            return crow::response(
                "<h1>Dashboard</h1>"
                "<a href='/logout'>Logout</a>"
            );
        }

        return crow::response(302).add_header("Location", "/login");
    });
}
