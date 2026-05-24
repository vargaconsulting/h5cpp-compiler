#include "pb_stub.hpp"

#include <cstdint>
#include <string>
#include <variant>

// Tier-3 fixture (stage 4): std::variant as proto3 oneof. The variant must
// start with std::monostate (the "absent" state); each subsequent alternative
// gets a field number from the [[clang::annotate("pb::oneof_tags=N1,N2,…")]]
// list on the variant member. Compiler emits a pb::oneof<&T::v,
// pb::alt<T1, N1>, ...>{} entry in the descriptor.
namespace sn::pb_test {

struct event_t {
    [[clang::annotate("pb::field=1")]] std::int64_t timestamp_ns;
    [[clang::annotate("pb::oneof_tags=5,6,7")]]
        std::variant<std::monostate, std::string, std::int64_t, double> payload;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::event_t e{};
    auto buf = pb::encode(e);
    (void)buf;
}
