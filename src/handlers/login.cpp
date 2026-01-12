#include "../handlers.hpp"
#include <nlohmann/json.hpp>

void register_login(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/login")
    ([&redis](const crow::request& req) {

        auto type = req.url_params.get("type");
        if (!type) {
            return crow::response(302).add_header("Location", "/");
        }

        std::string session = extract_session(req.get_header_value("Cookie"));
        std::string login_token = gen_uuid();

        if (session.empty()) {
            session = gen_uuid();
        }

        nlohmann::json value = {
            {"status", "anonymous"},
            {"login_token", login_token}
        };

        redis.set("session:" + session, value.dump());

        crow::response res(302);
        res.add_header("Set-Cookie", "SESSION=" + session + "; Path=/");
        res.add_header("Location", "http://auth/login?token=" + login_token);
        return res;
    });
}
