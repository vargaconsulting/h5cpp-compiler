#include "pb_stub.hpp"

#include <chrono>
#include <cstdint>

// Tier-4 fixture (stage 6): chrono adapter annotation. A user struct with
// std::chrono::system_clock::time_point + std::chrono::nanoseconds members
// gets pb::adapter_field<N, Ptr, pb::Timestamp_adapter> / Duration_adapter
// entries in the emitted descriptor. The library bridges the C++ types
// to proto3 google.protobuf.Timestamp / Duration wire messages.
namespace sn::pb_test {

struct event_t {
    [[clang::annotate("pb::field=1")]]
        std::int64_t id;

    [[clang::annotate("pb::field=2")]]
    [[clang::annotate("pb::adapter=Timestamp")]]
        std::chrono::system_clock::time_point when;

    [[clang::annotate("pb::field=3")]]
    [[clang::annotate("pb::adapter=Duration")]]
        std::chrono::nanoseconds ttl;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::event_t e{};
    auto buf = pb::encode(e);
    (void)buf;
}
