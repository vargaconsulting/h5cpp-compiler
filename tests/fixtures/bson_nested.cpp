#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[bson::doc("Inner nested struct")]] Inner {
    int value;
};

struct [[bson::doc("Nested struct test"), bson::alias("Outer")]] Record {
    Inner inner;
    int plain;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/nested", r);
}
