#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::chunk(256)]] chunked_t {
    unsigned long long timestamp_ns;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::chunked_t c;
    h5::write(0, "chunked", c);
}
