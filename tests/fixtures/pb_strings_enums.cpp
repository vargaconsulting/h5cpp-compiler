#include "pb_stub.hpp"

#include <cstdint>
#include <string>

// Tier-1 fixture (stage 2): non-POD members — std::string is the canonical
// length-delimited scalar; enum class is the canonical proto3 enum mapping.
// The compiler emits a single descriptor_t<log_entry_t> with one pb::field
// entry per member; std::string is NOT walked into (it's a stdlib leaf).
namespace sn::pb_test {

enum class severity_e : std::int32_t {
    UNSPECIFIED = 0,
    INFO        = 1,
    WARN        = 2,
    ERROR       = 3,
};

struct log_entry_t {
    [[pb::field(1)]] std::int64_t timestamp_ns;
    [[pb::field(2)]] std::string  message;
    [[pb::field(3)]] severity_e   level;
    [[pb::field(4)]] double       elapsed_ms;
};

} // namespace sn::pb_test

void use() {
    sn::pb_test::log_entry_t e{};
    auto buf = pb::encode(e);
    (void)buf;
}
