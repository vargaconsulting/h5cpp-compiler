#include <optional>
#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[rlp::doc("String and optional type test")]] Record {
    [[rlp::name("display_name")]]
    std::string label;
    std::optional<int> maybe_count;
    std::optional<std::string> maybe_label;
    [[rlp::required]]
    std::string id;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/strings", r);
}
