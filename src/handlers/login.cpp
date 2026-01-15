#include "../handlers.hpp"
#include "../session.hpp"
#include "../utils.hpp"
#include "../redis.hpp"
#include "../api/auth_client.hpp"

#include <crow.h>
#include <sw/redis++/redis++.h>
#include <exception>
#include <string>

namespace {
crow::response redirect_to(const std::string& location) {
    crow::response res(302);
    res.add_header("Location", location);
    return res;
}
} // namespace

void register_login(crow::SimpleApp& app, RedisClient& redis) {
    CROW_ROUTE(app, "/login")([&redis](const crow::request& req) {
        try {
            auto type = req.url_params.get("type");
            if (!type) {
                return redirect_to("/");
            }

            std::string session = extract_session(req.get_header_value("Cookie"));
            std::string login_token = gen_uuid();

            SessionData data;
            bool needs_new_session = session.empty();

            // --- DEBUG: Redis health + GET ---
            if (!session.empty()) {
                CROW_LOG_INFO << "LOGIN before redis.get key=session:" << session;
                auto existing = redis.get("session:" + session);
                CROW_LOG_INFO << "LOGIN after redis.get";

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

            // --- DEBUG: SET ---
            CROW_LOG_INFO << "LOGIN before redis.set key=session:" << session;
            redis.set("session:" + session, serialize_session(data));
            CROW_LOG_INFO << "LOGIN after redis.set";

            // --- Auth call ---
            std::string auth_url = get_env(
                "AUTH_URL",
                "https://religiose-multinodular-jaqueline.ngrok-free.dev"
            );
            AuthClient auth(auth_url);

            std::string type_value = type;
            bool is_code = (type_value == "code");

            std::string redirect_url;
            std::string code_value;

            if (type_value == "github" || type_value == "yandex") {
                auto url = auth.StartOAuth(type_value, login_token);
                if (!url) {
                    return crow::response(502, "Authorization service unavailable");
                }
                redirect_url = *url;
                if (redirect_url.empty()) {
                    return crow::response(502, "Authorization service returned empty url");
                }
            } else if (is_code) {
                auto code = auth.StartCode(login_token);
                if (!code) {
                    return crow::response(502, "Authorization service unavailable");
                }
                code_value = *code;
                if (code_value.empty()) {
                    return crow::response(502, "Authorization service returned empty code");
                }
            } else {
                return crow::response(400, "Unsupported login provider");
            }

            // --- Response ---
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

        } catch (const sw::redis::Error& e) {
            // redis++ throws sw::redis::Error derivatives
            CROW_LOG_ERROR << "LOGIN redis exception: " << e.what();
            return crow::response(500, std::string("LOGIN redis exception: ") + e.what());
        } catch (const std::exception& e) {
            CROW_LOG_ERROR << "LOGIN exception: " << e.what();
            return crow::response(500, std::string("LOGIN exception: ") + e.what());
        }
    });
}
