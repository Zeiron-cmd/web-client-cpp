#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

struct SessionData {
    std::string status;
    std::string login_token;
    std::string access_token;
    std::string refresh_token;
};

inline std::optional<SessionData> parse_session(const std::string& value) {
    nlohmann::json json = nlohmann::json::parse(value, nullptr, false);
    if (json.is_discarded() || !json.contains("status")) {
        return std::nullopt;
    }

    SessionData data;
    data.status = json.value("status", "");
    data.login_token = json.value("login_token", "");
    data.access_token = json.value("access_token", "");
    data.refresh_token = json.value("refresh_token", "");
    return data;
}

inline std::string serialize_session(const SessionData& data) {
    nlohmann::json json = {
        {"status", data.status},
        {"login_token", data.login_token},
        {"access_token", data.access_token},
        {"refresh_token", data.refresh_token},
    };
    return json.dump();
}
