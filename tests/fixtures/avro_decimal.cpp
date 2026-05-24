#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[avro::doc("Decimal type test")]] Record {
    [[avro::decimal(10, 2)]]
    double price;
    [[avro::decimal(5)]]
    float tax;
    std::string name;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/decimal", r);
}
