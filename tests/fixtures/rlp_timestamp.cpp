#include <chrono>
#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[rlp::doc("Timestamp type test")]] Record {
    [[rlp::timestamp]]
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::string name;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/timestamp", r);
}
