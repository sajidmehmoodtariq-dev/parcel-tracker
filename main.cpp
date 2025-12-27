#include <crow.h>
#include <fstream>
#include <sstream>

// Helper function to read file content
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    crow::SimpleApp app;

    // API endpoint to test connection
    CROW_ROUTE(app, "/api/test")
    ([](){
        crow::json::wvalue response;
        response["status"] = "connected";
        response["message"] = "Backend is connected!";
        return response;
    });

    // Serve index.html
    CROW_ROUTE(app, "/")
    ([](){
        std::string content = read_file("../public/index.html");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve CSS files
    CROW_ROUTE(app, "/styles.css")
    ([](){
        std::string content = read_file("../public/styles.css");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/css");
        return res;
    });

    // Serve JS files
    CROW_ROUTE(app, "/app.js")
    ([](){
        std::string content = read_file("../public/app.js");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "application/javascript");
        return res;
    });

    std::cout << "Server starting on http://localhost:18080" << std::endl;
    app.port(18080).multithreaded().run();
    
    return 0;
}
