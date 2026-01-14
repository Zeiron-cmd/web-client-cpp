#include "http.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <cctype>

namespace {
size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t write_header(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(buffer, size * nitems);

    auto pos = line.find(':');
    if (pos == std::string::npos) {
        return size * nitems;
    }

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);

    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    (*headers)[key] = value;
    return size * nitems;
}

curl_slist* build_headers(const std::vector<std::string>& headers) {
    curl_slist* list = nullptr;
    for (const auto& header : headers) {
        list = curl_slist_append(list, header.c_str());
    }
    return list;
}
} // namespace

HttpResponse http_request(const std::string& method,
                          const std::string& url,
                          const std::string& body,
                          const std::vector<std::string>& headers) {
    HttpResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.status = 0;
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

    curl_slist* header_list = build_headers(headers);
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    }

    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);

    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);

    return response;
}

HttpResponse http_get(const std::string& url, const std::vector<std::string>& headers) {
    return http_request("GET", url, "", headers);
}

HttpResponse http_post_json(const std::string& url,
                            const std::string& body,
                            const std::vector<std::string>& headers) {
    std::vector<std::string> merged_headers = headers;
    merged_headers.push_back("Content-Type: application/json");
    return http_request("POST", url, body, merged_headers);
}
