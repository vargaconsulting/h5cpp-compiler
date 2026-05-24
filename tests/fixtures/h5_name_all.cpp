#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::name_all("pfx_", "_sfx")]] name_all_t {
    unsigned long long timestamp_ns;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::name_all_t n;
    h5::write(0, "name_all", n);
}
