// daemons/video/src/video_daemon.cpp
//
// C++ MJPEG video daemon for Raspberry Pi camera using rpicam-* tools.
// Endpoints:
//   GET /health        -> {"status":"ok"}
//   GET /snapshot.jpg  -> single JPEG
//   GET /mjpeg         -> multipart/x-mixed-replace MJPEG stream
//
// Build: via CMakeLists.txt in daemons/video
// Run:   ./video_daemon
//
// Env vars:
//   VIDEO_PORT (default 8080)
//   VIDEO_WIDTH (default 640)
//   VIDEO_HEIGHT (default 480)
//   VIDEO_FPS (default 30)
//   RPICAM_VID (default "rpicam-vid")
//   RPICAM_JPEG (default "rpicam-jpeg")
//   RPICAM_VID_FLAGS (default "")
//   RPICAM_JPEG_FLAGS (default "")

#include <cstdio>     // FILE, popen, pclose, fread, feof, ferror
#include <cstdlib>    // getenv
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../third_party/httplib.h"

// ---------------------------
// Env helpers
// ---------------------------
static int env_int(const char* key, int defv) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defv;
    try { return std::stoi(v); } catch (...) { return defv; }
}

static std::string env_str(const char* key, const std::string& defv) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defv;
    return std::string(v);
}

// Minimal safe quoting for shell execution: wrap arg in single quotes
static std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

// Read all bytes from a FILE* into a vector
static std::vector<uint8_t> read_all(FILE* fp) {
    std::vector<uint8_t> buf;
    uint8_t tmp[8192];
    while (true) {
        size_t n = std::fread(tmp, 1, sizeof(tmp), fp);
        if (n > 0) buf.insert(buf.end(), tmp, tmp + n);
        if (n < sizeof(tmp)) {
            if (std::feof(fp)) break;
            if (std::ferror(fp)) break;
        }
    }
    return buf;
}

// Find a complete JPEG frame in a rolling buffer by SOI/EOI markers.
// Returns true and sets [frame_start, frame_end) if found.
static bool find_jpeg_frame(const std::vector<uint8_t>& data,
                            size_t start,
                            size_t& frame_start,
                            size_t& frame_end) {
    // Find SOI (FFD8)
    size_t i = start;
    for (; i + 1 < data.size(); ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD8) {
            frame_start = i;
            break;
        }
    }
    if (i + 1 >= data.size()) return false;

    // Find EOI (FFD9)
    for (size_t j = frame_start + 2; j + 1 < data.size(); ++j) {
        if (data[j] == 0xFF && data[j + 1] == 0xD9) {
            frame_end = j + 2;  // exclusive
            return true;
        }
    }
    return false;
}

int main() {
    const int port   = env_int("VIDEO_PORT", 8080);
    const int width  = env_int("VIDEO_WIDTH", 640);
    const int height = env_int("VIDEO_HEIGHT", 480);
    const int fps    = env_int("VIDEO_FPS", 30);

    const std::string rpicam_vid  = env_str("RPICAM_VID",  "rpicam-vid");
    const std::string rpicam_jpeg = env_str("RPICAM_JPEG", "rpicam-jpeg");

    const std::string extra_vid_flags  = env_str("RPICAM_VID_FLAGS",  "");
    const std::string extra_jpeg_flags = env_str("RPICAM_JPEG_FLAGS", "");

    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    server.Get("/snapshot.jpg", [&](const httplib::Request&, httplib::Response& res) {
        // One-shot JPEG to stdout
        std::ostringstream cmd;
        cmd << shell_quote(rpicam_jpeg)
            << " -n -t 1"
            << " --width " << width
            << " --height " << height
            << " " << extra_jpeg_flags
            << " -o -";

        FILE* fp = popen(cmd.str().c_str(), "r");
        if (!fp) {
            res.status = 500;
            res.set_content("{\"status\":\"error\",\"error\":\"popen failed\"}", "application/json");
            return;
        }

        auto bytes = read_all(fp);
        pclose(fp);

        if (bytes.size() < 2) {
            res.status = 500;
            res.set_content("{\"status\":\"error\",\"error\":\"snapshot produced no data\"}", "application/json");
            return;
        }

        res.status = 200;
        res.set_header("Cache-Control", "no-store");
        res.set_content(reinterpret_cast<const char*>(bytes.data()), bytes.size(), "image/jpeg");
    });

    server.Get("/mjpeg", [&](const httplib::Request&, httplib::Response& res) {
        // MJPEG stream as multipart/x-mixed-replace
        const std::string boundary = "FRAME";
        res.status = 200;
        res.set_header("Cache-Control", "no-cache, private");
        res.set_header("Pragma", "no-cache");
        res.set_header("Connection", "close");        

        std::ostringstream cmd;
        cmd << shell_quote(rpicam_vid)
            << " -n -t 0"
            << " --width " << width
            << " --height " << height
            << " --framerate " << fps
            << " --codec mjpeg"
            << " " << extra_vid_flags
            << " -o -";

        FILE* fp = popen(cmd.str().c_str(), "r");
        if (!fp) {
            res.status = 500;
            res.set_content("{\"status\":\"error\",\"error\":\"popen failed\"}", "application/json");
            return;
        }

        // Provider WITHOUT length: must return bool
        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=" + boundary,
            [fp, boundary](size_t /*offset*/, httplib::DataSink& sink) -> bool {
                std::vector<uint8_t> buffer;
                buffer.reserve(512 * 1024);

                uint8_t tmp[8192];

                while (true) {
                    size_t n = std::fread(tmp, 1, sizeof(tmp), fp);
                    if (n > 0) buffer.insert(buffer.end(), tmp, tmp + n);

                    // Extract as many complete JPEG frames as possible
                    while (true) {
                        size_t fs = 0, fe = 0;
                        if (!find_jpeg_frame(buffer, 0, fs, fe)) break;

                        const size_t frame_len = fe - fs;

                        std::ostringstream hdr;
                        hdr << "--" << boundary << "\r\n"
                            << "Content-Type: image/jpeg\r\n"
                            << "Content-Length: " << frame_len << "\r\n\r\n";

                        if (!sink.write(hdr.str().c_str(), hdr.str().size())) return false;
                        if (!sink.write(reinterpret_cast<const char*>(buffer.data() + fs), frame_len)) return false;
                        if (!sink.write("\r\n", 2)) return false;

                        // Remove consumed bytes up to end of frame
                        buffer.erase(buffer.begin(), buffer.begin() + fe);
                    }

                    // If rpicam-vid ended or errored, stop streaming
                    if (n == 0) {
                        if (std::feof(fp)) break;
                        if (std::ferror(fp)) break;
                    }

                    // Client disconnected?
                    if (!sink.is_writable()) break;
                }

                sink.done();
                return true;
            },
            [fp](bool /*success*/) {
                pclose(fp);
            }
        );
    });

    std::cout << "video-daemon listening on 0.0.0.0:" << port << "\n";
    std::cout << "Endpoints: /health, /snapshot.jpg, /mjpeg\n";
    std::cout << "Try: http://<PI_IP>:" << port << "/mjpeg\n";

    server.listen("0.0.0.0", port);
    return 0;
}
