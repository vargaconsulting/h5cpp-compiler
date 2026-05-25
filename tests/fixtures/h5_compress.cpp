#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::compress("gzip", 6)]] compressed_t {
    unsigned long long timestamp_ns;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::compressed_t c;
    h5::write(0, "compressed", c);
}
