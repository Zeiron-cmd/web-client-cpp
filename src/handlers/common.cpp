#include "common.hpp"

#include "../http.hpp"
#include "../session.hpp"
#include "../utils.hpp"
#include <nlohmann/json.hpp>

namespace {
std::string auth_base_url() {
    return get_env("AUTH_URL", "http://auth:8081");
}

std::string main_base_url() {
    auto configured = get_env("MAIN_BASE_URL", "");
    if (!configured.empty()) {
        return configured;
    }
    return get_env("MAIN_URL", "http://main:8082");
}

crow::response redirect_to(const std::string& location) {
    return crow::response(302).add_header("Location", location);
}

crow::response login_page() {
    return crow::response(
        "<h1>Login</h1>"
        "<a href='/login?type=github'>GitHub</a><br>"
        "<a href='/login?type=yandex'>Yandex</a><br>"
        "<a href='/login?type=code'>Code</a>"
    );
}

crow::response dashboard_page() {
    return crow::response(
        "<h1>Dashboard</h1>"
        "<a href='/logout'>Logout</a><br>"
        "<a href='/logout?all=true'>Logout everywhere</a>"
    );
}

std::string path_only(const std::string& url) {
    auto pos = url.find('?');
    if (pos == std::string::npos) {
        return url;
    }
    return url.substr(0, pos);
}

crow::response handle_authorized(const crow::request& req,
                                RedisClient& redis,
                                const std::string& session_key,
                                SessionData session) {
    std::string path = path_only(req.url);
    if (path == "/") {
        return dashboard_page();
    }
    if (path == "/login") {
        return redirect_to("/");
    }

    if (path == "/logout") {
        return redirect_to("/");
    }

    if (req.method != crow::HTTPMethod::GET && req.method != crow::HTTPMethod::POST) {
        return crow::response(405);
    }

    std::string main_url = main_base_url() + req.url;
    std::vector<std::string> headers = {"Authorization: Bearer " + session.access_token};

    std::string method = req.method == crow::HTTPMethod::GET ? "GET" : "POST";
    auto main_response = http_request(method, main_url, req.body, headers);

    if (main_response.status == 401) {
        redis.del(session_key);
        return redirect_to("/");
    }

    if (main_response.status == 403) {
        return crow::response(403, "<h1>Access denied</h1>");
    }

    crow::response res(static_cast<int>(main_response.status));
    res.write(main_response.body);

    auto content_type = main_response.headers.find("content-type");
    if (content_type != main_response.headers.end()) {
        res.set_header("Content-Type", content_type->second);
    }

    return res;
}

crow::response handle_anonymous(const crow::request& req,
                               RedisClient& redis,
                               const std::string& session_key,
                               SessionData session) {
    std::string path = path_only(req.url);
    if (path == "/") {
        return login_page();
    }

    if (session.login_token.empty()) {
        redis.del(session_key);
        return redirect_to("/");
    }

    std::string check_url = auth_base_url() + "/auth/status?token_login=" + session.login_token;
    auto check_response = http_get(check_url);

    if (check_response.status != 200) {
        return redirect_to("/");
    }

    std::string status = "pending";
    std::string access_token;
    std::string refresh_token;

    auto auth_payload = nlohmann::json::parse(check_response.body, nullptr, false);
    if (!auth_payload.is_discarded()) {
        status = auth_payload.value("status", "pending");
        access_token = auth_payload.value("access_token", "");
        refresh_token = auth_payload.value("refresh_token", "");
    } else if (!check_response.body.empty()) {
        status = check_response.body;
    }
    if (status == "approved" || status == "success") {
        if (access_token.empty() || refresh_token.empty()) {
            return redirect_to("/");
        }

        session.status = "authorized";
        session.access_token = access_token;
        session.refresh_token = refresh_token;
        session.login_token.clear();

        redis.set(session_key, serialize_session(session));
        return handle_authorized(req, redis, session_key, session);
    }

    if (status == "denied" || status == "expired") {
        redis.del(session_key);
        return redirect_to("/");
    }

    return login_page();
}
} // namespace

crow::response handle_request(const crow::request& req, RedisClient& redis) {
    std::string path = path_only(req.url);

    std::string session = extract_session(req.get_header_value("Cookie"));
    if (session.empty()) {
        if (path == "/") {
            return login_page();
        }
        return redirect_to("/");
    }

    std::string session_key = "session:" + session;
    auto data = redis.get(session_key);
    if (!data) {
        return redirect_to("/");
    }

    auto session_data = parse_session(*data);
    if (!session_data) {
        redis.del(session_key);
        return redirect_to("/");
    }

    if (session_data->status == "authorized") {
        return handle_authorized(req, redis, session_key, *session_data);
    }

    return handle_anonymous(req, redis, session_key, *session_data);
}

void register_catchall(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/<path>")
    ([&redis](const crow::request& req) {
        return handle_request(req, redis);
    });
}
