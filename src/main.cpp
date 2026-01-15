#include <crow.h>
#include "redis.hpp"
#include "handlers.hpp"

int main() {
    crow::SimpleApp app;
    RedisClient redis;

    register_root(app, redis);
    register_login(app, redis);
    register_logout(app, redis);
    register_catchall(app, redis);

    app.bindaddr("0.0.0.0").port(8080).multithreaded().run();

}
