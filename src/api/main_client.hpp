#pragma once
#include <string>
#include <vector>

struct MainResult {
    int status = 0;
    std::string body;
    // если надо — позже добавим content-type
};

class MainClient {
public:
    explicit MainClient(std::string base_url);

    MainResult Do(const std::string& method,
                  const std::string& path,
                  const std::string& body,
                  const std::string& access_token);

private:
    std::string base;
    static std::string TrimRightSlash(std::string s);
};
