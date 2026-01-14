#pragma once
#include <crow.h>
#include "redis.hpp"
#include "utils.hpp"

void register_root(crow::SimpleApp& app, RedisClient& redis);
void register_login(crow::SimpleApp& app, RedisClient& redis);
void register_logout(crow::SimpleApp& app, RedisClient& redis);
void register_catchall(crow::SimpleApp& app, RedisClient& redis);
