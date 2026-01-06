#include <crow/app.h>

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        return "<h1>Hello Web Client</h1>";
    });

    app.port(8080).multithreaded().run();
}
