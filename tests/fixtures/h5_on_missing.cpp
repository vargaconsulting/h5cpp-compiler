#include "h5cpp_stub.hpp"
#include <string>
#include <vector>

namespace sn::sensor {
struct [[h5::on_missing("ignore")]] on_missing_t {
    unsigned long long timestamp_ns;
    std::vector<double> samples;
};
}

void use() {
    sn::sensor::on_missing_t o;
    h5::write(0, "on_missing", o);
}
