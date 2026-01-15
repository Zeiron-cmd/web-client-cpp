#include "common.hpp"

#include "../redis.hpp"
#include "../session.hpp"
#include "../utils.hpp"

#include "../api/auth_client.hpp"
#include "../api/main_client.hpp"

namespace {

std::string auth_base_url() {
    return get_env("AUTH_URL", "https://religiose-multinodular-jaqueline.ngrok-free.dev");
}

std::string main_base_url() {
    // MAIN_BASE_URL не нужен — используем только MAIN_URL
    return get_env("MAIN_URL", "https://shabbiest-continuately-zulma.ngrok-free.dev");
}

crow::response redirect_to(const std::string& location) {
    crow::response res(302);
    res.add_header("Location", location);
    return res;
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
    if (pos == std::string::npos) return url;
    return url.substr(0, pos);
}

crow::response handle_authorized(const crow::request& req,
                                RedisClient& redis,
                                const std::string& session_key,
                                SessionData session) {
    const std::string path = path_only(req.url);

    if (path == "/") {
        return dashboard_page();
    }
    if (path == "/login") {
        return redirect_to("/");
    }

    if (req.method != crow::HTTPMethod::GET && req.method != crow::HTTPMethod::POST) {
        return crow::response(405);
    }

    const std::string method = (req.method == crow::HTTPMethod::GET) ? "GET" : "POST";

    MainClient main(main_base_url());
    auto main_result = main.Do(method, req.url, req.body, session.access_token);

    if (main_result.status == 401) {
        // Refresh once
        AuthClient auth(auth_base_url());
        auto refreshed = auth.Refresh(session.refresh_token);
        if (!refreshed) {
            redis.del(session_key);
            return redirect_to("/");
        }

        session.access_token = refreshed->access_token;
        session.refresh_token = refreshed->refresh_token;
        redis.set(session_key, serialize_session(session));

        // retry once
        main_result = main.Do(method, req.url, req.body, session.access_token);

        if (main_result.status == 401) {
            redis.del(session_key);
            return redirect_to("/");
        }
    }

    if (main_result.status == 403) {
        return crow::response(403, "<h1>Access denied</h1>");
    }

    crow::response res(main_result.status);
    res.write(main_result.body);
    return res;
}

crow::response handle_anonymous(const crow::request& req,
                               RedisClient& redis,
                               const std::string& session_key,
                               SessionData session) {
    const std::string path = path_only(req.url);

    // Если пользователь зашёл на /login?type=... — этим занимается login.cpp (отдельный роут).
    // Здесь catchall, поэтому просто дадим ему отработать или редирект на /
    if (path == "/login") {
        return redirect_to("/");
    }

    // Если нет login_token — сессия битая/неполная
    if (session.login_token.empty()) {
        redis.del(session_key);
        return redirect_to("/");
    }

    // Проверяем статус авторизации в Auth-модуле
    AuthClient auth(auth_base_url());
    auto st = auth.Status(session.login_token);

    // Если Auth недоступен — по сценарию можно редирект/ожидание.
    // Я оставляю редирект на / (чтобы пользователь мог повторить).
    if (!st) {
        return redirect_to("/");
    }

    const std::string status = st->status;

    if (status == "approved" || status == "success") {
        if (st->access_token.empty() || st->refresh_token.empty()) {
            // Некорректный ответ Auth — откатываем
            return redirect_to("/");
        }

        session.status = "authorized";
        session.access_token = st->access_token;
        session.refresh_token = st->refresh_token;
        session.login_token.clear();

        redis.set(session_key, serialize_session(session));

        // Продолжаем обработку запроса как authorized (в т.ч. для "/")
        return handle_authorized(req, redis, session_key, session);
    }

    if (status == "denied" || status == "expired") {
        redis.del(session_key);
        return redirect_to("/");
    }

    // pending / unknown: показываем login page (включая "/")
    return login_page();
}


} // namespace

crow::response handle_request(const crow::request& req, RedisClient& redis) {
    const std::string path = path_only(req.url);

    // Нет cookie -> неизвестный пользователь
    std::string session = extract_session(req.get_header_value("Cookie"));
    if (session.empty()) {
        if (path == "/") return login_page();
        return redirect_to("/");
    }

    const std::string session_key = "session:" + session;

    // Сессии в Redis нет -> неизвестный пользователь
    auto data = redis.get(session_key);
    if (!data) {
        if (path == "/") return login_page();
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
    ([&redis](const crow::request& req, const std::string&) {
        return handle_request(req, redis);
    });
}
