#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[json::doc("String type test")]] Record {
    std::string raw_text;
    [[json::name("display_name")]]
    std::string label;
    [[json::format("email")]]
    std::string email;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/strings", r);
}
