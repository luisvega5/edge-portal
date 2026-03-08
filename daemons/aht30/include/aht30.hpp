#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

struct Aht30Reading
{
    double temperature_c;
    double humidity_percent;
    bool busy_bit_set;
};

class Aht30
{
public:
    // Opens the I2C device and sets the slave address
    Aht30(const std::string& i2c_device, uint8_t address);

    // Automatically closes the file descriptor when the object is destroyed
    ~Aht30();

    // Trigger a measurement, wait, read, and verify CRC
    Aht30Reading read();

    // Non-copyable: avoid double-close on the fd
    Aht30(const Aht30&) = delete;
    Aht30& operator=(const Aht30&) = delete;

    // Movable: allow ownership transfer
    Aht30(Aht30&& other) noexcept;
    Aht30& operator=(Aht30&& other) noexcept;

private:
    int fd_{-1};
    std::string dev_;
    uint8_t addr_{0};

    static uint8_t crc8_aht(const uint8_t* data, size_t len);
};
