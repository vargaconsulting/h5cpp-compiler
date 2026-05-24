#include "pb_stub.hpp"

#include <cstdint>
#include <string>

// Phase 2 (issue #31): universal attributes pb::name, pb::doc, pb::on_missing.
//
// These are spec'd in tasks/h5cpp-compiler-pb-attribute-taxonomy.md as the
// shared vocabulary across HDF5 and protobuf backends. For the .hpp shim
// they emit as leading-comment metadata above the descriptor and its
// entries. The Phase 3 .proto text emitter consumes the same parsers to
// produce real schema output (field renames, trailing comments, message-
// scope doc strings).

namespace sn::pb_test {

struct [[pb::name("UserProfile"),
        pb::doc("user-level profile aggregate captured by the gateway")]]
user_profile_t {
    [[pb::field(1), pb::doc("display name; UTF-8")]]
        std::string  name;

    [[pb::field(2), pb::name("user_id_v2"), pb::doc("opaque server-side ID")]]
        std::int64_t user_id;

    [[pb::field(3), pb::on_missing(0.5)]]
        double       confidence;     // proto3 zero-default overridden via on_missing

    [[pb::field(4), pb::on_missing("anonymous")]]
        std::string  display;

    [[pb::field(5), pb::doc("granted at signup"), pb::on_missing(0)]]
        std::int32_t score;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::user_profile_t p{};
    auto buf = pb::encode(p);
    (void)buf;
}
