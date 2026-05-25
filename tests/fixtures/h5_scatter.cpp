#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct session_t {
    unsigned long long timestamp_ns;
    std::string label;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::session_t s;
    h5::write(0, "sessions", s);
}
