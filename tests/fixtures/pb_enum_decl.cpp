#include "pb_stub.hpp"

#include <cstdint>

// Commit 1: proto3 `enum Foo { VAL = N; ... }` declaration emission.
//
// Three scenarios in one fixture:
//   1. `color_e` — has an UNSPECIFIED = 0 entry already. Clean emission;
//                  no diagnostic.
//   2. `priority_e` — no zero-valued enumerator BUT carries
//                     [[pb::enum_zero("UNSPECIFIED")]]. h5cpp synthesizes
//                     `UNSPECIFIED = 0;` as the first line of the proto3
//                     enum block. The C++ enum is unchanged.
//   3. `direction_e` — uses [[pb::name(...)]] to override the proto3
//                      message name. Already has a zero entry.

namespace sn::pb_test {

enum class color_e : std::int32_t {
    UNSPECIFIED = 0,
    RED         = 1,
    GREEN       = 2,
    BLUE        = 3,
};

// Proto3 requires the first entry to be 0-valued; this C++ enum has no
// zero. The pb::enum_zero override synthesizes one in the .proto output.
enum class [[pb::enum_zero("PRIORITY_UNSPECIFIED")]] priority_e : std::int32_t {
    LOW    = 1,
    NORMAL = 2,
    HIGH   = 3,
};

enum class [[pb::name("Heading"), pb::doc("compass headings")]] direction_e : std::int32_t {
    HEADING_UNSPECIFIED = 0,
    NORTH               = 1,
    EAST                = 2,
    SOUTH               = 3,
    WEST                = 4,
};

struct event_t {
    [[pb::field(1)]] color_e     color;
    [[pb::field(2)]] priority_e  priority;
    [[pb::field(3)]] direction_e heading;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::event_t e{};
    auto buf = pb::encode(e);
    (void)buf;
}
