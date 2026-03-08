#include "aht30.hpp"

#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <exception>

#include "../third_party/httplib.h"

static std::string iso8601_utc_now() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static std::string json_escape(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '\\': oss << "\\\\"; break;
            case '"':  oss << "\\\""; break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:   oss << c;      break;
        }
    }
    return oss.str();
}

int main() {
    // One sensor instance for the life of the daemon
    Aht30 sensor("/dev/i2c-1", 0x38);
    std::mutex sensor_mutex;

    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
        res.status = 200;
    });

    server.Get("/read", [&sensor, &sensor_mutex](const httplib::Request&, httplib::Response& res) {
        try {
            Aht30Reading reading;
            {
                std::lock_guard<std::mutex> lock(sensor_mutex);
                reading = sensor.read();
            }

            const double temp_f = reading.temperature_c * 9.0 / 5.0 + 32.0;

            std::ostringstream out;
            out << std::fixed << std::setprecision(2);
            out << "{"
                << "\"status\":\"ok\","
                << "\"timestamp\":\"" << iso8601_utc_now() << "\","
                << "\"temp_c\":" << reading.temperature_c << ","
                << "\"temp_f\":" << temp_f << ","
                << "\"humidity\":" << reading.humidity_percent << ","
                << "\"busy\":" << (reading.busy_bit_set ? "true" : "false")
                << "}";

            res.set_content(out.str(), "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            std::ostringstream err;
            err << "{"
                << "\"status\":\"error\","
                << "\"timestamp\":\"" << iso8601_utc_now() << "\","
                << "\"error\":\"" << json_escape(e.what()) << "\""
                << "}";

            res.set_content(err.str(), "application/json");
            res.status = 500;
        }
    });

    std::cout << "AHT30 daemon listening on 0.0.0.0:7070\n";
    server.listen("0.0.0.0", 7070);
    return 0;
}
