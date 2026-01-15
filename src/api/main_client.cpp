#include "main_client.hpp"
#include "../http.hpp"

MainClient::MainClient(std::string base_url)
    : base(TrimRightSlash(std::move(base_url))) {}

std::string MainClient::TrimRightSlash(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

MainResult MainClient::Do(const std::string& method,
                          const std::string& path,
                          const std::string& body,
                          const std::string& access_token) {
    std::vector<std::string> headers;
    if (!access_token.empty()) {
        headers.push_back("Authorization: Bearer " + access_token);
    }
    auto resp = http_request(method, base + path, body, headers);
    return MainResult{resp.status, resp.body};
}
