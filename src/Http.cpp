// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Http.h"

#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "LuaAPI.h"

#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

using json = nlohmann::json;

struct Connection {
    std::string host {};
    int port {};
    Connection() = default;
    Connection(std::string host, int port)
        : host(host)
        , port(port) {};
};

constexpr uint8_t CONNECTION_AMOUNT = 10;
static thread_local uint8_t write_index = 0;
static thread_local std::array<Connection, CONNECTION_AMOUNT> connections;
static thread_local std::array<std::shared_ptr<httplib::SSLClient>, CONNECTION_AMOUNT> clients;

[[nodiscard]] static std::shared_ptr<httplib::SSLClient> getClient(Connection connectionInfo) {
    for (uint8_t i = 0; i < CONNECTION_AMOUNT; i++) {
        if (connectionInfo.host == connections[i].host
            && connectionInfo.port == connections[i].port) {
            beammp_tracef("Old client reconnected, with ip {} and port {}", connectionInfo.host, connectionInfo.port);
            return clients[i];
        }
    }
    uint8_t i = write_index;
    write_index++;
    write_index %= CONNECTION_AMOUNT;
    clients[i] = std::make_shared<httplib::SSLClient>(connectionInfo.host, connectionInfo.port);
    connections[i] = { connectionInfo.host, connectionInfo.port };
    beammp_tracef("New client connected, with ip {} and port {}", connectionInfo.host, connectionInfo.port);
    return clients[i];
}

std::string Http::GET(const std::string& host, int port, const std::string& target, unsigned int* status) {
    std::shared_ptr<httplib::SSLClient> client = getClient({ host, port });
    client->enable_server_certificate_verification(false);
    client->set_address_family(AF_UNSPEC);
    auto res = client->Get(target.c_str());
    if (res) {
        if (status) {
            *status = res->status;
        }
        return res->body;
    } else {
        return Http::ErrorString;
    }
}

std::string Http::POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status, const httplib::Headers& headers) {
    std::shared_ptr<httplib::SSLClient> client = getClient({ host, port });
    client->set_read_timeout(std::chrono::seconds(10));
    beammp_assert(client->is_valid());
    client->enable_server_certificate_verification(false);
    client->set_address_family(AF_UNSPEC);
    auto res = client->Post(target.c_str(), headers, body.c_str(), body.size(), ContentType.c_str());
    if (res) {
        if (status) {
            *status = res->status;
        }
        return res->body;
    } else {
        beammp_debug("POST failed: " + httplib::to_string(res.error()));
        return Http::ErrorString;
    }
}

// RFC 2616, RFC 7231
static std::map<size_t, const char*> Map = {
    { -1, "Invalid Response Code" },
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 102, "Processing" },
    { 103, "Early Hints" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 207, "Multi-Status" },
    { 208, "Already Reported" },
    { 226, "IM Used" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 306, "(Unused)" },
    { 307, "Temporary Redirect" },
    { 308, "Permanent Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Payload Too Large" },
    { 414, "URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 421, "Misdirected Request" },
    { 422, "Unprocessable Entity" },
    { 423, "Locked" },
    { 424, "Failed Dependency" },
    { 425, "Too Early" },
    { 426, "Upgrade Required" },
    { 428, "Precondition Required" },
    { 429, "Too Many Requests" },
    { 431, "Request Header Fields Too Large" },
    { 451, "Unavailable For Legal Reasons" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
    { 506, "Variant Also Negotiates" },
    { 507, "Insufficient Storage" },
    { 508, "Loop Detected" },
    { 510, "Not Extended" },
    { 511, "Network Authentication Required" },
    // cloudflare status codes
    { 520, "(CDN) Web Server Returns An Unknown Error" },
    { 521, "(CDN) Web Server Is Down" },
    { 522, "(CDN) Connection Timed Out" },
    { 523, "(CDN) Origin Is Unreachable" },
    { 524, "(CDN) A Timeout Occurred" },
    { 525, "(CDN) SSL Handshake Failed" },
    { 526, "(CDN) Invalid SSL Certificate" },
    { 527, "(CDN) Railgun Listener To Origin Error" },
    { 530, "(CDN) 1XXX Internal Error" },
};

static const char Magic[] = {
    0x20, 0x2f, 0x5c, 0x5f,
    0x2f, 0x5c, 0x0a, 0x28,
    0x20, 0x6f, 0x2e, 0x6f,
    0x20, 0x29, 0x0a, 0x20,
    0x3e, 0x20, 0x5e, 0x20,
    0x3c, 0x0a, 0x00
};

std::string Http::Status::ToString(int Code) {
    if (Map.find(Code) != Map.end()) {
        return Map.at(Code);
    } else {
        return std::to_string(Code);
    }
}

TEST_CASE("Http::Status::ToString") {
    CHECK(Http::Status::ToString(200) == "OK");
    CHECK(Http::Status::ToString(696969) == "696969");
    CHECK(Http::Status::ToString(-1) == "Invalid Response Code");
}

Http::Server::THttpServerInstance::THttpServerInstance() {
    Application::SetSubsystemStatus("HTTPServer", Application::Status::Starting);
    mThread = std::thread(&Http::Server::THttpServerInstance::operator(), this);
    mThread.detach();
}

void Http::Server::THttpServerInstance::operator()() try {
    std::unique_ptr<httplib::Server> HttpLibServerInstance;
    HttpLibServerInstance = std::make_unique<httplib::Server>();
    // todo: make this IP agnostic so people can set their own IP
    HttpLibServerInstance->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("<!DOCTYPE html><article><h1>Hello World!</h1><section><p>BeamMP Server can now serve HTTP requests!</p></section></article></html>", "text/html");
    });
    HttpLibServerInstance->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        size_t SystemsGood = 0;
        size_t SystemsBad = 0;
        auto Statuses = Application::GetSubsystemStatuses();
        for (const auto& NameStatusPair : Statuses) {
            switch (NameStatusPair.second) {
            case Application::Status::Starting:
            case Application::Status::ShuttingDown:
            case Application::Status::Shutdown:
            case Application::Status::Good:
                SystemsGood++;
                break;
            case Application::Status::Bad:
                SystemsBad++;
                break;
            default:
                beammp_assert_not_reachable();
            }
        }
        res.set_content(
            json {
                { "ok", SystemsBad == 0 },
            }
                .dump(),
            "application/json");
        res.status = 200;
    });
    // magic endpoint
    HttpLibServerInstance->Get({ 0x2f, 0x6b, 0x69, 0x74, 0x74, 0x79 }, [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(Magic), "text/plain");
    });
    HttpLibServerInstance->set_logger([](const httplib::Request& Req, const httplib::Response& Res) {
        beammp_debug("Http Server: " + Req.method + " " + Req.target + " -> " + std::to_string(Res.status));
    });
    Application::SetSubsystemStatus("HTTPServer", Application::Status::Good);
} catch (const std::exception& e) {
    beammp_error("Failed to start http server. Please ensure the http server is configured properly in the ServerConfig.toml, or turn it off if you don't need it. Error: " + std::string(e.what()));
}
