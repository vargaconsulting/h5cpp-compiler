#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[bson::doc("Binary type test")]] Record {
    [[bson::binary(4)]]
    std::vector<unsigned char> uuid;
    [[bson::binary(0)]]
    std::vector<unsigned char> raw;
    std::string name;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/binary", r);
}
