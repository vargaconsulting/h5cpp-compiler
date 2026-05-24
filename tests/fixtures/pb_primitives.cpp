#include "pb_stub.hpp"

#include <cstdint>

// Tier-1 fixture: numeric scalars only. Every member carries
// [[clang::annotate("pb::field=N")]]; the compiler emits a
// pb::meta::descriptor_t<T> specialization with one pb::field<N,&T::m>{}
// entry per member, in source-declaration order.
namespace sn::pb_test {
struct primitives_t {
    [[clang::annotate("pb::field=1")]] bool         _bool;
    [[clang::annotate("pb::field=2")]] std::int32_t _i32;
    [[clang::annotate("pb::field=3")]] std::int64_t _i64;
    [[clang::annotate("pb::field=4")]] std::uint32_t _u32;
    [[clang::annotate("pb::field=5")]] std::uint64_t _u64;
    [[clang::annotate("pb::field=6")]] float        _f;
    [[clang::annotate("pb::field=7")]] double       _d;
};
} // namespace sn::pb_test

void use() {
    sn::pb_test::primitives_t p{};
    auto buf = pb::encode(p);
    (void)buf;
}
