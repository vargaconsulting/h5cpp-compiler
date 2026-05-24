#include "pb_stub.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <variant>

// Issue #31 Phase 1b — clean `[[pb::field(N)]]` syntax delivered by the
// source-level rewriter (src/pb_attr_translator.hpp).
//
// No `clang::annotate(...)` envelope; users write attributes directly in
// the pb:: namespace. Tag arguments may be integer literals OR enum
// values OR any mix — the rewriter lowers everything to clang::annotate
// before Clang's AST walker sees it, and Expr::EvaluateAsInt resolves
// both kinds in one code path.

namespace acme::telemetry {

enum class telemetry_tag : std::uint32_t {
    captured_at = 1,
    elapsed     = 2,
    device_id   = 3,
    samples     = 4,
    payload     = 5,         // base for oneof: 5/6/7
};

struct telemetry_event_t {
    [[pb::field(telemetry_tag::captured_at), pb::adapter("Timestamp")]]
        std::chrono::system_clock::time_point captured_at;

    [[pb::field(telemetry_tag::elapsed), pb::adapter("Duration")]]
        std::chrono::nanoseconds elapsed;

    [[pb::field(telemetry_tag::device_id)]]
        std::string device_id;

    [[pb::field(telemetry_tag::samples)]]
        std::map<std::string, double> samples;

    [[pb::field(telemetry_tag::payload,
                static_cast<std::uint32_t>(telemetry_tag::payload) + 1u,
                static_cast<std::uint32_t>(telemetry_tag::payload) + 2u)]]
        std::variant<std::monostate,
                      std::string,       // text alert  (tag 5)
                      std::int64_t,      // event code  (tag 6)
                      double>            // metric      (tag 7)
        payload;

    [[pb::field(10), pb::wire("sint32")]]
        std::int32_t delta;              // zigzag, often negative

    [[pb::field(11)]]
        std::int32_t legacy_count;       // raw int next to enum-tagged fields

    [[pb::ignore]]
        void* runtime_handle;            // cache; never persisted to wire
};

} // namespace acme::telemetry

void use() {
    acme::telemetry::telemetry_event_t e{};
    auto buf = pb::encode(e);
    (void)buf;
}
