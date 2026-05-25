#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::chunk(128), h5::compress("gzip", 9)]] chunked_compressed_t {
    unsigned long long timestamp_ns;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::chunked_compressed_t c;
    h5::write(0, "chunked_compressed", c);
}
