#include "redis.hpp"

RedisClient::RedisClient()
    : redis("tcp://redis:6379") {}

std::optional<std::string> RedisClient::get(const std::string& key) {
    auto val = redis.get(key);
    if (val) return *val;
    return std::nullopt;
}

void RedisClient::set(const std::string& key, const std::string& value) {
    redis.set(key, value);
}

void RedisClient::del(const std::string& key) {
    redis.del(key);
}
