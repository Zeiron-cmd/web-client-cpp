#include "redis.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

RedisClient::RedisClient() : host_("redis"), port_(6379) {}

namespace {

// Connect to host:port, return fd
int connect_tcp(const std::string& host, int port) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const std::string port_str = std::to_string(port);

    int rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0 || !res) {
        throw std::runtime_error("getaddrinfo failed for " + host + ":" + port_str + " : " + gai_strerror(rc));
    }

    int fd = -1;
    for (auto p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return fd;
        }
        ::close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    throw std::runtime_error("connect failed to " + host + ":" + port_str);
}

void send_all(int fd, const std::string& data) {
    const char* p = data.data();
    size_t left = data.size();
    while (left > 0) {
        ssize_t n = ::send(fd, p, left, 0);
        if (n <= 0) throw std::runtime_error("send failed");
        p += n;
        left -= static_cast<size_t>(n);
    }
}

// read exactly N bytes
std::string recv_n(int fd, size_t n) {
    std::string out;
    out.resize(n);
    size_t got = 0;
    while (got < n) {
        ssize_t r = ::recv(fd, &out[got], n - got, 0);
        if (r <= 0) throw std::runtime_error("recv failed");
        got += static_cast<size_t>(r);
    }
    return out;
}

// read until \r\n (returns line without CRLF)
std::string recv_line(int fd) {
    std::string line;
    char c;
    bool last_cr = false;
    while (true) {
        ssize_t r = ::recv(fd, &c, 1, 0);
        if (r <= 0) throw std::runtime_error("recv failed");
        if (last_cr) {
            if (c == '\n') {
                // drop the \r
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return line;
            }
            last_cr = false;
        }
        line.push_back(c);
        if (c == '\r') last_cr = true;
    }
}

// Build RESP array: ["CMD", arg1, arg2, ...]
std::string resp_array(const std::vector<std::string>& parts) {
    std::string out = "*" + std::to_string(parts.size()) + "\r\n";
    for (const auto& s : parts) {
        out += "$" + std::to_string(s.size()) + "\r\n";
        out += s;
        out += "\r\n";
    }
    return out;
}

// Parse minimal RESP replies:
//
// +OK\r\n                     -> status "OK"
// -ERR ...\r\n                -> throw
// :123\r\n                    -> integer 123
// $-1\r\n                     -> null bulk
// $<len>\r\n<bytes>\r\n        -> bulk string
//
struct RespReply {
    enum class Type { Status, Error, Integer, Bulk, NullBulk } type;
    std::string str;
    long long integer = 0;
};

RespReply read_reply(int fd) {
    std::string first = recv_n(fd, 1);
    char t = first[0];

    if (t == '+') {
        auto line = recv_line(fd);
        return {RespReply::Type::Status, line, 0};
    }
    if (t == '-') {
        auto line = recv_line(fd);
        return {RespReply::Type::Error, line, 0};
    }
    if (t == ':') {
        auto line = recv_line(fd);
        return {RespReply::Type::Integer, "", std::stoll(line)};
    }
    if (t == '$') {
        auto line = recv_line(fd);
        long long len = std::stoll(line);
        if (len == -1) {
            return {RespReply::Type::NullBulk, "", 0};
        }
        auto data = recv_n(fd, static_cast<size_t>(len));
        // trailing CRLF
        (void)recv_n(fd, 2);
        return {RespReply::Type::Bulk, data, 0};
    }

    throw std::runtime_error("unknown RESP reply type");
}

} // namespace

std::optional<std::string> RedisClient::get(const std::string& key) {
    int fd = connect_tcp(host_, port_);
    try {
        auto req = resp_array({"GET", key});
        send_all(fd, req);

        auto rep = read_reply(fd);
        ::close(fd);

        if (rep.type == RespReply::Type::Error) {
            throw std::runtime_error("Redis error: " + rep.str);
        }
        if (rep.type == RespReply::Type::NullBulk) {
            return std::nullopt;
        }
        if (rep.type != RespReply::Type::Bulk) {
            throw std::runtime_error("Unexpected reply type for GET");
        }
        return rep.str;

    } catch (...) {
        ::close(fd);
        throw;
    }
}

void RedisClient::set(const std::string& key, const std::string& value) {
    int fd = connect_tcp(host_, port_);
    try {
        auto req = resp_array({"SET", key, value});
        send_all(fd, req);

        auto rep = read_reply(fd);
        ::close(fd);

        if (rep.type == RespReply::Type::Error) {
            throw std::runtime_error("Redis error: " + rep.str);
        }
        if (rep.type != RespReply::Type::Status || rep.str != "OK") {
            throw std::runtime_error("Unexpected reply for SET");
        }
    } catch (...) {
        ::close(fd);
        throw;
    }
}

void RedisClient::del(const std::string& key) {
    int fd = connect_tcp(host_, port_);
    try {
        auto req = resp_array({"DEL", key});
        send_all(fd, req);

        auto rep = read_reply(fd);
        ::close(fd);

        if (rep.type == RespReply::Type::Error) {
            throw std::runtime_error("Redis error: " + rep.str);
        }
        // DEL returns integer, but we don't care
        if (rep.type != RespReply::Type::Integer) {
            throw std::runtime_error("Unexpected reply type for DEL");
        }
    } catch (...) {
        ::close(fd);
        throw;
    }
}
