#include "h5cpp_stub.hpp"
#include <optional>

namespace sn::typecheck {
struct [[cbor::doc("Optional types test")]] Record {
    std::optional<int> maybe_int;
    std::optional<std::string> maybe_str;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/optionals", r);
}
