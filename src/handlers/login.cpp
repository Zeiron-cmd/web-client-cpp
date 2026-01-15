#include "../handlers.hpp"
#include "../http.hpp"
#include "../session.hpp"
#include "../utils.hpp"
#include <nlohmann/json.hpp>
#include "../api/auth_client.hpp"


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
AuthClient auth(auth_url);

std::string type_value = type;
bool is_code = type_value == "code";

std::string redirect_url;
std::string code_value;

        if (type_value == "github" || type_value == "yandex") {
            auto url = auth.StartOAuth(type_value, login_token);
            if (!url) {
                return crow::response(502, "Authorization service unavailable");
            }
            redirect_url = *url;
        } else if (is_code) {
            auto code = auth.StartCode(login_token);
            if (!code) {
                return crow::response(502, "Authorization service unavailable");
            }
            code_value = *code;
        } else {
            return crow::response(400, "Unsupported login provider");
        }


        std::string response_body = auth_response.body;
        if (response_body.size() >= 2 && response_body.front() == '"' && response_body.back() == '"') {
            response_body = response_body.substr(1, response_body.size() - 2);
        }

        std::string redirect_url = response_body;
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

        if (redirect_url.empty()) {
            return crow::response(502, "Authorization service returned empty response");
        }

        crow::response res;
        res.add_header("Set-Cookie", "SESSION=" + session + "; Path=/; HttpOnly; SameSite=Lax");

        if (is_code) {
            res.code = 200;
            res.write("<h1>Code authentication</h1><p>Your code: <strong>" + code_value + "</strong></p>");
            return res;
        }
        

        res.code = 302;
        res.add_header("Location", redirect_url);
        return res;
    });
}
