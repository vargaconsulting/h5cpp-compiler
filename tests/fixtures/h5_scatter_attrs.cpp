#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct annotated_t {
    unsigned long long timestamp_ns;
    std::string label;
    [[h5::ignore]] std::vector<int> ignored_samples;
    [[h5::name("data")]] std::vector<double> readings;
};
}

void use() {
    sn::sensor::annotated_t a;
    h5::write(0, "annotated", a);
}
