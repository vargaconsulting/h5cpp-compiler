#include "pb_stub.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Tier-3 fixture (stage 3): composite shapes — std::vector (repeated),
// std::optional (proto3 optional explicit presence). The compiler emits the
// same pb::field<N, &T::m>{} entries; pb.hpp's trait dispatch handles the
// shape at compile time via is_pb_repeated_v and is_pb_optional_v.
//
// stdlib types are NOT walked into for descriptor emission (only the outer
// user record gets a descriptor_t<>).
namespace sn::pb_test {

struct profile_t {
    [[clang::annotate("pb::field=1")]] std::string                  name;
    [[clang::annotate("pb::field=2")]] std::vector<std::int32_t>    scores;
    [[clang::annotate("pb::field=3")]] std::optional<std::string>   nickname;
    [[clang::annotate("pb::field=4")]] std::vector<std::string>     aliases;
    [[clang::annotate("pb::field=5")]] std::optional<std::int64_t>  user_id;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::profile_t p{};
    auto buf = pb::encode(p);
    (void)buf;
}
