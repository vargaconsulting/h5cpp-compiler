#include <optional>
#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[avro::doc("String and optional type test")]] Record {
    [[avro::name("display_name")]]
    std::string label;
    std::optional<int> maybe_count;
    std::optional<std::string> maybe_label;
    [[avro::required]]
    std::string id;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/strings", r);
}
