#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct Inner {
    int x;
    double y;
};

struct [[rlp::doc("Nested type test")]] Record {
    Inner inner;
    std::vector<Inner> inners;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/nested", r);
}
