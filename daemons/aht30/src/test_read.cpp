#include "aht30.hpp"
#include <iostream>
#include <iomanip>

int main() {
    try {
        Aht30 sensor("/dev/i2c-1", 0x38);
        auto reading = sensor.read();

        const double temp_f = reading.temperature_c * 9.0 / 5.0 + 32.0;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Temp: " << reading.temperature_c << "°C ("
                  << temp_f << "°F), "
                  << "Humidity: " << reading.humidity_percent << "%";
        if (reading.busy_bit_set) {
            std::cout << " (BUSY bit set)";
        }
        std::cout << "\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
