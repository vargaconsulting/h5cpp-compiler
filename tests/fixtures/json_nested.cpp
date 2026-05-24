#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[json::doc("Inner nested struct")]] Inner {
    int value;
};

struct [[json::doc("Nested struct test"), json::alias("Outer")]] Record {
    Inner inner;
    int plain;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/nested", r);
}
