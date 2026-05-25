#include "pb_stub.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <variant>

// Issue #31 headline feature: enum-typed tag arguments.
//
// Tag numbers come from a named enum rather than scattered integer literals.
// The compiler evaluates each enum reference via Expr::EvaluateAsInt(ctx)
// and emits the same pb::field<N, Ptr>{} descriptor as if the user had
// written the raw number. Mixed enum + int args allowed (see `legacy_count`
// which uses a raw integer right alongside the enum-tagged fields).
//
// Permissive default per the attribute taxonomy doc §6 — no class-level
// "must use this enum" binding required; the library doesn't choose the route.

namespace sn::pb_test {

enum class user_profile_tag : std::uint32_t {
    name      = 1,
    scores    = 2,
    flags     = 3,
    payload   = 5,                   // base for oneof — alts at 5, 6, 7
};

struct user_profile_t {
    [[pb::field(user_profile_tag::name)]]
        std::string                                          name;

    [[pb::field(user_profile_tag::scores)]]
        std::map<std::string, std::int32_t>                  scores;

    [[pb::field(user_profile_tag::flags)]]
        std::map<bool, std::string>                          flags;

    // Variadic enum-tagged oneof. Three alternatives mapped to a contiguous
    // tag range. Compiler reads each Expr* via EvaluateAsInt → integer 5/6/7.
    [[pb::field(user_profile_tag::payload,
                static_cast<std::uint32_t>(user_profile_tag::payload) + 1u,
                static_cast<std::uint32_t>(user_profile_tag::payload) + 2u)]]
        std::variant<std::monostate, std::string, std::int64_t, double> payload;

    // Mixed-form: raw integer alongside enum-tagged fields. Permissive default
    // accepts the mix without diagnostic.
    [[pb::field(10)]]
        std::int32_t legacy_count;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::user_profile_t u{};
    auto buf = pb::encode(u);
    (void)buf;
}
