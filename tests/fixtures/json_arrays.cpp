#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct Inner {
    int value;
};

struct [[json::doc("Array types test")]] Record {
    std::vector<int> int_vec;
    int int_arr[4];
    std::vector<Inner> inner_vec;
    Inner inner_arr[2];
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/arrays", r);
}
