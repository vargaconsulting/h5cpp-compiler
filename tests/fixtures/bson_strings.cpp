#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[bson::doc("String type test")]] Record {
    std::string raw_text;
    [[bson::name("display_name")]]
    std::string label;
    [[bson::required]]
    std::string email;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/strings", r);
}
