#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[msgpack::doc("String type test")]] Record {
    std::string raw_text;
    [[msgpack::name("display_name")]]
    std::string label;
    [[msgpack::required]]
    std::string email;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/strings", r);
}
