#include "pb_stub.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Phase 4 (issue #31): Tier 2 attribute surface — every entry from the
// taxonomy doc §3.2 exercised on one structure. Drives:
//   - class-level pb::package    → file-scope `package com.acme.events;`
//   - class-level pb::reserved   → `reserved N1, N2; reserved "name";`
//   - class-level pb::version    → `option (h5cpp.schema_version) = 2;`
//   - class-level pb::name_all   → snake_case naming convention
//                                   applied to each field
//   - field-level pb::packed     → `[packed = true]` option
//   - field-level pb::deprecated → `[deprecated = true]` option
//   - field-level pb::alias      → aggregated `reserved "old_name";`
//                                   at message scope

namespace acme::events {

struct [[pb::package("com.acme.events"),
        pb::name("UserEvent"),
        pb::name_all("snake_case"),
        pb::reserved(10, 11, "obsolete_field"),
        pb::version(2),
        pb::doc("user-facing event captured by the gateway")]]
user_event_t {
    [[pb::field(1), pb::doc("nanoseconds since epoch")]]
        std::int64_t timestamp_ns;

    // [[pb::name(...)]] takes precedence over name_all; explicit override.
    [[pb::field(2), pb::name("user_id"), pb::doc("opaque server-side ID")]]
        std::uint32_t userId;

    [[pb::field(3), pb::packed, pb::doc("packed numeric samples")]]
        std::vector<std::int32_t> samples;

    // legacy field marked deprecated but still emitted on the wire.
    [[pb::field(4), pb::deprecated]]
        std::int32_t legacy_count;

    // pb::alias for backward-name compat with a previous schema; emitted
    // as `reserved "old_temperature";` at message scope so the old name
    // can't be reused by a new field later.
    [[pb::field(5), pb::name("temperature_k"), pb::alias("old_temperature")]]
        double temperature;

    // mixed: name_all snake_cases this CamelCase member to `display_name`.
    [[pb::field(6)]]
        std::string displayName;
};

} // namespace acme::events

void use() {
    acme::events::user_event_t e{};
    auto buf = pb::encode(e);
    (void)buf;
}
