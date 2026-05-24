#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[cbor::doc("String type test")]] Record {
    std::string raw_text;
    [[cbor::name("display_name")]]
    std::string label;
    [[cbor::required]]
    std::string email;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/strings", r);
}
