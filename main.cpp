#include <crow/app.h>

int main() {
    crow::SimpleApp app;
    
    CROW_ROUTE(app, "/")([](){
        return "<h1>Hello Web Client</h1>";
    });

    CROW_ROUTE(app, "/me")([](const crow::request& req){
        auto cookie = req.get_header_value("Cookie");

        if (cookie.find("session_token=") == std::string::npos) {
            return crow::response(401, "Unauthorized");
        }

        return crow::response(200, "You are authorized");
    });
    CROW_ROUTE(app, "/logout")([](){
        crow::response res("Logged out");
        res.add_header("Set-Cookie", "session_token=; Max-Age=0");
        return res;
    });
    CROW_ROUTE(app, "/login")([](){
        crow::response res;
        res.code = 302;
        res.add_header("Location", "https://religiose-multinodular-jaqueline.ngrok-free.dev/docs#/Auth/github_oauth_url_auth_github_url_get");
        return res;
    });



    app.port(8080).multithreaded().run();
}
