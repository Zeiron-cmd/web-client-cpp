#include "../handlers.hpp"
#include "../http.hpp"
#include "../session.hpp"
#include "../utils.hpp"
#include <nlohmann/json.hpp>

namespace {
crow::response redirect_to(const std::string& location) {
    crow::response res(302);
    res.add_header("Location", location);
    return res;
}
} // namespace

void register_login(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/login")
    ([&redis](const crow::request& req) {

        auto type = req.url_params.get("type");
        if (!type) {
            return redirect_to("/");
        }

        std::string session = extract_session(req.get_header_value("Cookie"));
        std::string login_token = gen_uuid();

        SessionData data;
        bool needs_new_session = session.empty();

        if (!session.empty()) {
            auto existing = redis.get("session:" + session);
            if (existing) {
                auto parsed = parse_session(*existing);
                if (parsed && parsed->status == "authorized") {
                    return redirect_to("/");
                }
            } else {
                needs_new_session = true;
            }
        }

        if (needs_new_session) {
            session = gen_uuid();
        }

        data.status = "anonymous";
        data.login_token = login_token;
        data.access_token.clear();
        data.refresh_token.clear();

        redis.set("session:" + session, serialize_session(data));

        std::string auth_url = get_env("AUTH_URL", "https://religiose-multinodular-jaqueline.ngrok-free.dev");
        std::string type_value = type;
        std::string request_url;
        bool is_code = type_value == "code";

        if (type_value == "github" || type_value == "yandex") {
            request_url = auth_url + "/auth/oauth/start?provider=" + type_value + "&token_login=" + login_token;
        } else if (is_code) {
            request_url = auth_url + "/auth/code/start?token_login=" + login_token;
        } else {
            return crow::response(400, "Unsupported login provider");
        }

        auto auth_response = http_request("POST", request_url, "", {});
        if (auth_response.status < 200 || auth_response.status >= 400) {
            return crow::response(502, "Authorization service unavailable");
        }

        std::string response_body = auth_response.body;
        if (response_body.size() >= 2 && response_body.front() == '"' && response_body.back() == '"') {
            response_body = response_body.substr(1, response_body.size() - 2);
        }

        std::string redirect_url;
        auto location_header = auth_response.headers.find("location");
        if (auth_response.status >= 300 && auth_response.status < 400 && location_header != auth_response.headers.end()) {
            redirect_url = location_header->second;
        }

        if (redirect_url.empty()) {
            redirect_url = response_body;
        }
        auto parsed_payload = nlohmann::json::parse(auth_response.body, nullptr, false);
        if (!parsed_payload.is_discarded()) {
            if (parsed_payload.is_string()) {
                redirect_url = parsed_payload.get<std::string>();
            } else if (parsed_payload.is_object()) {
                for (const auto& key : {"url", "link", "redirect_url", "auth_url"}) {
                    auto it = parsed_payload.find(key);
                    if (it != parsed_payload.end() && it->is_string()) {
                        redirect_url = it->get<std::string>();
                        break;
                    }
                }
            }
        }

        crow::response res;
        res.add_header("Set-Cookie", "SESSION=" + session + "; Path=/");

        if (is_code) {
            if (redirect_url.empty() && response_body.empty()) {
                return crow::response(502, "Authorization service returned empty response");
            }
            if (!redirect_url.empty() && redirect_url != response_body) {
                res.code = 302;
                res.add_header("Location", redirect_url);
                return res;
            }

            res.code = 200;
            res.write("<h1>Code authentication</h1><p>Your code: <strong>" + response_body + "</strong></p>");
            return res;
        }

        if (redirect_url.empty()) {
            return crow::response(502, "Authorization service returned empty response");
        }

        res.code = 302;
        res.add_header("Location", redirect_url);
        return res;
    });
}
