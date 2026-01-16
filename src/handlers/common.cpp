#include "common.hpp"
#include <nlohmann/json.hpp>

#include <vector>
#include <string>

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

// --- safe HTML helpers ---

std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;";  break;
            case '>': out += "&gt;";  break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

crow::response html_response(const std::string& html) {
    crow::response res(html);
    res.add_header("Content-Type", "text/html; charset=utf-8");
    return res;
}

std::string wrap_html(const std::string& title, const std::string& body) {
    return
        "<!doctype html>"
        "<html lang='ru'>"
        "<head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>" + html_escape(title) + "</title>"
        "</head>"
        "<body>" + body + "</body>"
        "</html>";
}

// --- pages ---

crow::response login_page() {
    std::string body =
        std::string("<h1>Login</h1>")
        + "<a href='/login?type=github'>GitHub</a><br>"
        + "<a href='/login?type=yandex'>Yandex</a><br>"
        + "<a href='/login?type=code'>Code</a>";

    return html_response(wrap_html("Login", body));
}

crow::response dashboard_page() {
    std::string body =
        std::string("<h1>Dashboard</h1>")
        + "<a href='/logout'>Logout</a><br>"
        + "<a href='/logout?all=true'>Logout everywhere</a>";

    return html_response(wrap_html("Dashboard", body));
}

std::string path_only(const std::string& url) {
    auto pos = url.find('?');
    if (pos == std::string::npos) return url;
    return url.substr(0, pos);
}

std::string method_to_string(crow::HTTPMethod m) {
    switch (m) {
        case crow::HTTPMethod::GET:     return "GET";
        case crow::HTTPMethod::POST:    return "POST";
        case crow::HTTPMethod::PUT:     return "PUT";
        case crow::HTTPMethod::DELETE:  return "DELETE";
        case crow::HTTPMethod::PATCH:   return "PATCH";
        case crow::HTTPMethod::HEAD:    return "HEAD";
        case crow::HTTPMethod::OPTIONS: return "OPTIONS";
        default: return "";
    }
}

struct MainCallResult {
    int status;
    std::string body;
};

MainCallResult main_get_with_refresh(
    const std::string& url,
    RedisClient& redis,
    const std::string& session_key,
    SessionData& session
) {
    MainClient main(main_base_url());
    auto r = main.Do("GET", url, "", session.access_token);

    if (r.status != 401) {
        return {r.status, r.body};
    }

    // 401 → пробуем refresh один раз
    AuthClient auth(auth_base_url());
    auto refreshed = auth.Refresh(session.refresh_token);
    if (!refreshed) {
        redis.del(session_key);
        return {401, ""};
    }

    session.access_token = refreshed->access_token;
    session.refresh_token = refreshed->refresh_token;
    redis.set(session_key, serialize_session(session));

    // retry
    r = main.Do("GET", url, "", session.access_token);
    if (r.status == 401) {
        redis.del(session_key);
        return {401, ""};
    }

    return {r.status, r.body};
}

// --- JSON helpers ---

std::vector<nlohmann::json> json_as_list(const nlohmann::json& j) {
    if (j.is_array()) return j.get<std::vector<nlohmann::json>>();

    if (j.is_object()) {
        for (const char* key : {"items", "results", "data"}) {
            auto it = j.find(key);
            if (it != j.end() && it->is_array()) {
                return it->get<std::vector<nlohmann::json>>();
            }
        }
    }
    return {};
}

std::string json_get_str(const nlohmann::json& o, const std::vector<std::string>& keys) {
    for (const auto& k : keys) {
        auto it = o.find(k);
        if (it != o.end()) {
            if (it->is_string()) return it->get<std::string>();
            if (it->is_number_integer()) return std::to_string(it->get<long long>());
            if (it->is_number_unsigned()) return std::to_string(it->get<unsigned long long>());
        }
    }
    return "";
}

std::string link_list_from_json(const std::string& title,
                               const std::string& raw_json,
                               const std::string& base_path,
                               const std::string& id_param,
                               const std::vector<std::string>& id_keys,
                               const std::vector<std::string>& label_keys) {
    std::string html;
    html += "<h2>" + html_escape(title) + "</h2>";

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(raw_json);
    } catch (...) {
        html += "<p><b>Не смог распарсить JSON</b></p>";
        html += "<pre>" + html_escape(raw_json) + "</pre>";
        return html;
    }

    auto items = json_as_list(j);
    if (items.empty()) {
        html += "<pre>" + html_escape(raw_json) + "</pre>";
        return html;
    }

    html += "<ul>";
    for (const auto& it : items) {
        if (!it.is_object()) continue;

        std::string id = json_get_str(it, id_keys);
        std::string label = json_get_str(it, label_keys);
        if (label.empty()) label = id.empty() ? "(item)" : ("ID " + id);

        if (!id.empty()) {
            html += "<li><a href='" + base_path + "?" + id_param + "=" + id + "'>"
                 + html_escape(label) + "</a></li>";
        } else {
            html += "<li>" + html_escape(label) + "</li>";
        }
    }
    html += "</ul>";

    return html;
}

crow::response dashboard_page_with_data(const std::string& courses_json,
                                       const std::string& notif_json,
                                       const std::string* users_json_or_null) {
    std::string html;

    html += "<h1>Dashboard</h1>";
    html += "<a href='/logout'>Logout</a><br>";
    html += "<a href='/logout?all=true'>Logout everywhere</a>";
    html += "<hr>";

    // Навигация
    html += "<h2>Навигация</h2>";
    html += "<ul>";
    html += "<li><a href='/courses'>Courses</a></li>";
    html += "<li><a href='/notifications'>Notifications</a></li>";
    html += "<li><a href='/users'>Users</a></li>";
    html += "</ul>";
    html += "<hr>";

    html += link_list_from_json(
        "Courses (кликабельно, если есть id/course_id)",
        courses_json,
        "/course",
        "course_id",
        {"course_id", "id"},
        {"name", "title", "description"}
    );

    html += "<h2>Notifications</h2>";
    html += "<pre>" + html_escape(notif_json) + "</pre>";

    if (users_json_or_null) {
        html += link_list_from_json(
            "Users (кликабельно, если есть id)",
            *users_json_or_null,
            "/user",
            "id",
            {"id", "user_id"},
            {"fullName", "full_name", "name", "fio"}
        );
    } else {
        html += "<h2>Users</h2><p><i>Нет доступа или endpoint недоступен</i></p>";
    }

    return html_response(wrap_html("Dashboard", html));
}

// --- END helpers ---

crow::response handle_authorized(const crow::request& req,
                                RedisClient& redis,
                                const std::string& session_key,
                                SessionData session) {
    const std::string path = path_only(req.url);

    // Dashboard: интеграция с Main (и refresh работает через helper)
    if (path == "/") {

        auto courses = main_get_with_refresh("/courses_list", redis, session_key, session);
        if (courses.status == 401) return redirect_to("/");
        if (courses.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        auto notif = main_get_with_refresh("/notification", redis, session_key, session);
        if (notif.status == 401) return redirect_to("/");
        if (notif.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        auto users = main_get_with_refresh("/users_list", redis, session_key, session);
        if (users.status == 401) return redirect_to("/");

        if (users.status == 403 || users.status < 200 || users.status >= 300) {
            return dashboard_page_with_data(courses.body, notif.body, nullptr);
        }
        return dashboard_page_with_data(courses.body, notif.body, &users.body);
    }

    if (path == "/login") {
        return redirect_to("/");
    }

    // Списки
    if (path == "/courses") {
        auto r = main_get_with_refresh("/courses_list", redis, session_key, session);
        if (r.status == 401) return redirect_to("/");
        if (r.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        std::string body;
        body += "<h1>Courses</h1><a href='/'>Back</a><hr>";
        body += link_list_from_json("Courses", r.body, "/course", "course_id",
                                   {"course_id","id"}, {"name","title","description"});

        return html_response(wrap_html("Courses", body));
    }

    if (path == "/users") {
        auto r = main_get_with_refresh("/users_list", redis, session_key, session);
        if (r.status == 401) return redirect_to("/");
        if (r.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        std::string body;
        body += "<h1>Users</h1><a href='/'>Back</a><hr>";
        body += link_list_from_json("Users", r.body, "/user", "id",
                                   {"id","user_id"}, {"fullName","full_name","name","fio"});

        return html_response(wrap_html("Users", body));
    }

    if (path == "/notifications") {
        auto r = main_get_with_refresh("/notification", redis, session_key, session);
        if (r.status == 401) return redirect_to("/");
        if (r.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        std::string body;
        body += "<h1>Notifications</h1><a href='/'>Back</a><hr>";
        body += "<pre>" + html_escape(r.body) + "</pre>";

        return html_response(wrap_html("Notifications", body));
    }

    // Детали
    if (path == "/course") {
        auto course_id = req.url_params.get("course_id");
        if (!course_id) return html_response(wrap_html("Bad request", "<h1>course_id required</h1>"));

        std::string url = std::string("/course_get?course_id=") + course_id;
        auto r = main_get_with_refresh(url, redis, session_key, session);
        if (r.status == 401) return redirect_to("/");
        if (r.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        std::string body;
        body += "<h1>Course</h1><a href='/courses'>Back</a><hr>";
        body += "<pre>" + html_escape(r.body) + "</pre>";

        return html_response(wrap_html("Course", body));
    }

    if (path == "/user") {
        auto id = req.url_params.get("id");
        if (!id) return html_response(wrap_html("Bad request", "<h1>id required</h1>"));

        std::string url = std::string("/user_get?id=") + id;
        auto r = main_get_with_refresh(url, redis, session_key, session);
        if (r.status == 401) return redirect_to("/");
        if (r.status == 403) return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));

        std::string body;
        body += "<h1>User</h1><a href='/users'>Back</a><hr>";
        body += "<pre>" + html_escape(r.body) + "</pre>";

        return html_response(wrap_html("User", body));
    }

    // Остальные URL: общий прокси в Main как было
    const std::string method = method_to_string(req.method);
    if (method.empty()) {
        return crow::response(405);
    }

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
        return html_response(wrap_html("Access denied", "<h1>Access denied</h1>"));
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

    if (path == "/login") {
        return redirect_to("/");
    }

    if (session.login_token.empty()) {
        redis.del(session_key);
        return redirect_to("/");
    }

    AuthClient auth(auth_base_url());
    auto st = auth.Status(session.login_token);

    if (!st) {
        return redirect_to("/");
    }

    const std::string status = st->status;

    if (status == "approved" || status == "success") {
        if (st->access_token.empty() || st->refresh_token.empty()) {
            return redirect_to("/");
        }

        session.status = "authorized";
        session.access_token = st->access_token;
        session.refresh_token = st->refresh_token;
        session.login_token.clear();

        redis.set(session_key, serialize_session(session));

        return handle_authorized(req, redis, session_key, session);
    }

    if (status == "denied" || status == "expired") {
        redis.del(session_key);
        return redirect_to("/");
    }

    if (path == "/") {
        return login_page();
    }
    return redirect_to("/");
}

} // namespace

crow::response handle_request(const crow::request& req, RedisClient& redis) {
    const std::string path = path_only(req.url);

    std::string session = extract_session(req.get_header_value("Cookie"));
    if (session.empty()) {
        if (path == "/") return login_page();
        return redirect_to("/");
    }

    const std::string session_key = "session:" + session;

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
    CROW_CATCHALL_ROUTE(app)
    ([&redis](const crow::request& req) {
        return handle_request(req, redis);
    });
}
