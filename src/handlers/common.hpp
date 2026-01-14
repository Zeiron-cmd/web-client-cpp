#pragma once

#include <crow.h>
#include "../redis.hpp"

crow::response handle_request(const crow::request& req, RedisClient& redis);
void register_catchall(crow::SimpleApp& app, RedisClient& redis);
