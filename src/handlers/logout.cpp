#include "../handlers.hpp"
#include "../session.hpp"
#include "../utils.hpp"

namespace {
crow::response redirect_to_root() {
    crow::response res(302);
    res.add_header("Location", "/");
    return res;
}
} // namespace

void register_logout(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/logout")
    ([&redis](const crow::request& req) {

        std::string session = extract_session(req.get_header_value("Cookie"));
        if (session.empty()) {
            return redirect_to_root();
        }

        std::string session_key = "session:" + session;
        auto data = redis.get(session_key);
        if (!data) {
            return redirect_to_root();
        }

        auto parsed = parse_session(*data);
        (void)parsed;

        redis.del(session_key);
        return redirect_to_root();
    });
}
