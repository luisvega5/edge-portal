#include "aht30.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

static const uint8_t MEASURE_CMD[3] = { 0xAC, 0x33, 0x00 };
static const int MEASURE_DELAY_US = 80000; // ~80ms

Aht30::Aht30(const std::string& i2c_device, uint8_t address)
    : dev_(i2c_device), addr_(address)
{
    fd_ = ::open(dev_.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to open I2C device: " + dev_);
    }

    if (::ioctl(fd_, I2C_SLAVE, addr_) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::system_error(errno, std::generic_category(), "Failed to set I2C address");
    }
}

Aht30::~Aht30()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Aht30::Aht30(Aht30&& other) noexcept
    : fd_(other.fd_), dev_(std::move(other.dev_)), addr_(other.addr_)
{
    other.fd_ = -1;
    other.addr_ = 0;
}

Aht30& Aht30::operator=(Aht30&& other) noexcept
{
    if (this == &other) return *this;

    if (fd_ >= 0) {
        ::close(fd_);
    }

    fd_ = other.fd_;
    dev_ = std::move(other.dev_);
    addr_ = other.addr_;

    other.fd_ = -1;
    other.addr_ = 0;

    return *this;
}

uint8_t Aht30::crc8_aht(const uint8_t* data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x80) crc = static_cast<uint8_t>((crc << 1) ^ 0x31);
            else           crc = static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

Aht30Reading Aht30::read()
{
    if (::write(fd_, MEASURE_CMD, sizeof(MEASURE_CMD)) != static_cast<ssize_t>(sizeof(MEASURE_CMD))) {
        throw std::system_error(errno, std::generic_category(), "Failed to write measurement command");
    }

    ::usleep(MEASURE_DELAY_US);

    uint8_t buf[7] = {0};
    if (::read(fd_, buf, sizeof(buf)) != static_cast<ssize_t>(sizeof(buf))) {
        throw std::system_error(errno, std::generic_category(), "Failed to read data from sensor");
    }

    const bool busy = (buf[0] & 0x80) != 0;

    if (crc8_aht(buf, 6) != buf[6]) {
        throw std::runtime_error("CRC mismatch");
    }

    const uint32_t rh_raw =
        (static_cast<uint32_t>(buf[1]) << 12) |
        (static_cast<uint32_t>(buf[2]) << 4)  |
        ((static_cast<uint32_t>(buf[3]) >> 4) & 0x0F);

    const uint32_t t_raw =
        ((static_cast<uint32_t>(buf[3]) & 0x0F) << 16) |
        (static_cast<uint32_t>(buf[4]) << 8) |
        (static_cast<uint32_t>(buf[5]));

    const double humidity = (rh_raw / 1048576.0) * 100.0;
    const double tempC    = (t_raw  / 1048576.0) * 200.0 - 50.0;

    return { tempC, humidity, busy };
}
