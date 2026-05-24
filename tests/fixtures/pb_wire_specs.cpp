#include "pb_stub.hpp"

#include <cstdint>

// Tier-2 fixture (stage 5): [[pb::wire(spec)]] annotation. C++ int32_t maps
// to any of int32/sint32/fixed32/sfixed32 on the wire depending on the
// annotation. The compiler emits the WireSpec template arg on pb::field<>.
namespace sn::pb_test {

struct deltas_t {
    // Default (no pb::wire) → natural mapping (int32, varint with sign extension).
    [[pb::field(1)]]
        std::int32_t  natural;

    // sint32: zigzag varint — better for centered-around-zero distributions.
    [[pb::field(2), pb::wire("sint32")]]
        std::int32_t  zigzag;

    // fixed32: always 4 bytes LE (for uint32_t).
    [[pb::field(3), pb::wire("fixed32")]]
        std::uint32_t lo32;

    // sfixed64: always 8 bytes LE signed.
    [[pb::field(4), pb::wire("sfixed64")]]
        std::int64_t  big_signed;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::deltas_t d{};
    auto buf = pb::encode(d);
    (void)buf;
}
