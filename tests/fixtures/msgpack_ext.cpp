#include "h5cpp_stub.hpp"
#include <cstdint>

namespace sn::typecheck {
struct [[msgpack::ext(1)]] timestamp_t {
    std::int64_t seconds;
    std::int32_t nanos;
};

struct [[msgpack::doc("Extension type test")]] Record {
    [[msgpack::required]]
    timestamp_t when;
    std::string name;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/ext", r);
}
