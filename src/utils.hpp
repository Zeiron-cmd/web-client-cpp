#pragma once
#include <string>
#include <uuid/uuid.h>
#include <cstdlib>

inline std::string gen_uuid() {
    uuid_t id;
    uuid_generate(id);
    char out[37];
    uuid_unparse(id, out);
    return std::string(out);
}

inline std::string extract_session(const std::string& cookie) {
    auto pos = cookie.find("SESSION=");
    if (pos == std::string::npos) return "";
    auto end = cookie.find(";", pos);
    return cookie.substr(pos + 8, end - pos - 8);
}

inline std::string get_env(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    if (!value) return fallback;
    return std::string(value);
}
