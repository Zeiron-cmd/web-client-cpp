#pragma once

#include <map>
#include <string>
#include <vector>

struct HttpResponse {
    long status = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

HttpResponse http_request(const std::string& method,
                          const std::string& url,
                          const std::string& body,
                          const std::vector<std::string>& headers);

HttpResponse http_get(const std::string& url,
                      const std::vector<std::string>& headers = {});

HttpResponse http_post_json(const std::string& url,
                            const std::string& body,
                            const std::vector<std::string>& headers = {});
