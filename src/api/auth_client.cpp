#include "auth_client.hpp"
#include "../http.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cctype>

AuthClient::AuthClient(std::string base_url)
    : base(TrimRightSlash(std::move(base_url))) {}

std::string AuthClient::TrimRightSlash(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string AuthClient::UrlEncode(const std::string& s) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') {
            out << c;
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    return out.str();
}

std::optional<std::string> AuthClient::StartOAuth(const std::string& provider,
                                                  const std::string& token_login) {
    std::string url = base + "/auth/oauth/start?provider=" + UrlEncode(provider)
                    + "&token_login=" + UrlEncode(token_login);

    auto resp = http_request("POST", url, "", {});
    if (resp.status != 200) return std::nullopt;

    auto j = nlohmann::json::parse(resp.body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    auto it = j.find("url");
    if (it == j.end() || !it->is_string()) return std::nullopt;

    return it->get<std::string>();
}

std::optional<std::string> AuthClient::StartCode(const std::string& token_login) {
    std::string url = base + "/auth/code/start?token_login=" + UrlEncode(token_login);

    auto resp = http_request("POST", url, "", {});
    if (resp.status != 200) return std::nullopt;

    auto j = nlohmann::json::parse(resp.body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    auto it = j.find("code");
    if (it == j.end() || !it->is_string()) return std::nullopt;

    return it->get<std::string>();
}

std::optional<AuthStatus> AuthClient::Status(const std::string& token_login) {
    std::string url = base + "/auth/status?token_login=" + UrlEncode(token_login);

    auto resp = http_request("GET", url, "", {});
    if (resp.status != 200) return std::nullopt;

    auto j = nlohmann::json::parse(resp.body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    AuthStatus out;
    out.status = j.value("status", "");
    out.access_token = j.value("access_token", "");
    out.refresh_token = j.value("refresh_token", "");
    out.reason = j.value("reason", "");
    return out;
}

std::optional<AuthRefresh> AuthClient::Refresh(const std::string& refresh_token) {
    if (refresh_token.empty()) return std::nullopt;

    nlohmann::json body;
    body["refresh_token"] = refresh_token;

    std::string url = base + "/auth/refresh";
    auto resp = http_post_json(url, body.dump(), {});
    if (resp.status != 200) return std::nullopt;

    auto j = nlohmann::json::parse(resp.body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    AuthRefresh out;
    out.access_token = j.value("access_token", "");
    out.refresh_token = j.value("refresh_token", "");
    if (out.access_token.empty() || out.refresh_token.empty()) return std::nullopt;
    return out;
}
