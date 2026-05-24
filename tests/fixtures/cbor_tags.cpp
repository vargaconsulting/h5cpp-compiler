#include "h5cpp_stub.hpp"
#include <cstdint>

namespace sn::typecheck {
struct [[cbor::tag(1)]] timestamp_t {
    std::int64_t seconds;
    std::int32_t nanos;
};

struct [[cbor::doc("Tag type test")]] Record {
    [[cbor::required]]
    timestamp_t when;
    std::string name;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/tags", r);
}
