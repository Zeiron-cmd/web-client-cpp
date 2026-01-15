#include "redis.hpp"
#include <mutex>

RedisClient::RedisClient()
    : redis("tcp://redis:6379") {}

std::optional<std::string> RedisClient::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx);
    auto val = redis.get(key);
    if (val) return *val;
    return std::nullopt;
}

void RedisClient::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mtx);
    redis.set(key, value);
}

void RedisClient::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx);
    redis.del(key);
}
