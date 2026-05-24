#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::serialize_full]] serialize_full_t {
    unsigned long long timestamp_ns;
    std::string label;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::serialize_full_t s;
    h5::write(0, "serialize_full", s);
}
