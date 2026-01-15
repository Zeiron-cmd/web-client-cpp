#pragma once
#include <optional>
#include <string>

class RedisClient {
public:
    RedisClient();

    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, const std::string& value);
    void del(const std::string& key);

private:
    std::string host_;
    int port_;
};
