#pragma once
#include <string>
#include <optional>

struct AuthStatus {
    std::string status;        // "pending"/"approved"/"denied"/etc
    std::string access_token;
    std::string refresh_token;
    std::string reason;
};

struct AuthRefresh {
    std::string access_token;
    std::string refresh_token;
};

class AuthClient {
public:
    explicit AuthClient(std::string base_url);

    // POST /auth/oauth/start?provider=github|yandex&token_login=...
    // -> {"url":"https://github.com/login/oauth/authorize?..."}
    std::optional<std::string> StartOAuth(const std::string& provider,
                                          const std::string& token_login);

    // POST /auth/code/start?token_login=...
    // -> {"code":"1234"}
    std::optional<std::string> StartCode(const std::string& token_login);

    // GET /auth/status?token_login=...
    std::optional<AuthStatus> Status(const std::string& token_login);

    // POST /auth/refresh  body {"refresh_token":"..."}
    std::optional<AuthRefresh> Refresh(const std::string& refresh_token);

private:
    std::string base;

    static std::string TrimRightSlash(std::string s);
    static std::string UrlEncode(const std::string& s);
};
