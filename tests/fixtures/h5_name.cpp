#include "h5cpp_stub.hpp"

// Issue #32 — [[h5::name("...")]] renames a field for on-disk storage.

struct sensor_reading_t {
    unsigned long long timestamp_ns;
    [[h5::name("temp_K")]]
        double temperature;
    [[h5::name("pressure_Pa")]]
        float pressure;
    int    flags;
};

void use() {
    sensor_reading_t r{};
    h5::write(0, "readings", r);
}
