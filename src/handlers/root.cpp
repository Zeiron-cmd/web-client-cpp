#include "../handlers.hpp"
#include "common.hpp" // где объявлен handle_request

CROW_ROUTE(app, "/")
    .methods(crow::HTTPMethod::GET)
    ([&redis](const crow::request& req) {
        return handle_request(req, redis);
    });


