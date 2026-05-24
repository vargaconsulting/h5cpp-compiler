#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[msgpack::doc("Inner nested struct")]] Inner {
    int value;
};

struct [[msgpack::doc("Nested struct test"), msgpack::alias("Outer")]] Record {
    Inner inner;
    int plain;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/nested", r);
}
