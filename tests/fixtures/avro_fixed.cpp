#include <array>
#include <cstdint>
#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[avro::doc("Fixed type test")]] Record {
    [[avro::fixed("MD5", 16)]]
    std::array<std::uint8_t, 16> md5;
    [[avro::fixed("UUID")]]
    std::array<std::uint8_t, 16> uuid;
    std::string name;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/fixed", r);
}
