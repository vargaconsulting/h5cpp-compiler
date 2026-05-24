#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[cbor::doc("Inner nested struct")]] Inner {
    int value;
};

struct [[cbor::doc("Nested struct test"), cbor::alias("Outer")]] Record {
    Inner inner;
    int plain;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/nested", r);
}
