#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::doc("Sensor session data"), h5::alias("Session"), h5::version("1")]] metadata_t {
    unsigned long long timestamp_ns;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::metadata_t m;
    h5::write(0, "metadata", m);
}
