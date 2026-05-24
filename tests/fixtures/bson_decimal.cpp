#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[bson::doc("Decimal type test")]] Record {
    [[bson::decimal]]
    std::string price;
    int quantity;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/decimal", r);
}
