#include "pb_stub.hpp"

#include <cstdint>
#include <string>

// Commit 2: Tier-3a + Tier-4-trivials attribute surface.
//
//   pb::json_name("alt_json")     — field option: [json_name = "..."]
//   pb::target_syntax("proto3")   — file-scope syntax line selector
//   pb::descriptor_set_out("...") — trailing protoc-invocation hint
//   pb::reject                    — skip this record entirely; emits a
//                                    diagnostic on the .hpp side and is
//                                    absent from .proto.
//
// The first three exercise on `event_t`; `legacy_t` is rejected as the
// fourth scenario.

namespace sn::pb_test {

struct [[pb::name("Event"),
        pb::target_syntax("proto3"),
        pb::descriptor_set_out("events.desc"),
        pb::doc("user-facing event with JSON-side aliasing")]]
event_t {
    [[pb::field(1)]] std::int64_t  timestamp_ns;

    [[pb::field(2), pb::json_name("userId")]]
        std::int64_t                user_id;

    [[pb::field(3), pb::json_name("displayName"), pb::doc("UTF-8 display name")]]
        std::string                 display_name;
};

// pb::reject — proves the class-level skip. Should produce a diagnostic
// on the .hpp side AND be absent from the .proto.
struct [[pb::reject, pb::doc("HDF5-only — never on the wire")]] legacy_internal_t {
    [[pb::field(1)]] std::int32_t internal_counter;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::event_t e{};
    auto buf = pb::encode(e);
    (void)buf;
    sn::pb_test::legacy_internal_t l{};
    auto buf2 = pb::encode(l);  // triggers pb::reject diagnostic
    (void)buf2;
}
