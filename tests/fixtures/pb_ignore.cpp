#include "pb_stub.hpp"

#include <cstdint>
#include <string>

// FR12: [[pb::ignore]] marks a non-static data member as deliberately
// excluded from wire encoding (caches, derived state, locks, runtime
// handles, etc.). The compiler emits pb::ignore<&T::m>{} in the
// descriptor — required so CCC#7 descriptor-completeness recognizes the
// member as deliberately omitted rather than accidentally forgotten.

namespace sn::pb_test {

struct record_with_ignore_t {
    [[pb::field(1)]]                std::string  name;
    [[pb::ignore]]                  void*        runtime_handle;   // cache; never wire
    [[pb::field(2)]]                std::int32_t score;
    [[pb::ignore]]                  std::int64_t derived_counter;  // computed at runtime
    [[pb::field(3)]]                std::string  description;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::record_with_ignore_t r{};
    auto buf = pb::encode(r);
    (void)buf;
}
