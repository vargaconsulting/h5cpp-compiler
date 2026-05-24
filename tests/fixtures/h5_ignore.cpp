#include "h5cpp_stub.hpp"

// Issue #32 — [[h5::ignore]] skips a field entirely.

struct session_t {
    unsigned long long id;
    double temperature;
    [[h5::ignore]]
        int debug_counter;
    [[h5::ignore]]
        int cache_line;
};

void use() {
    session_t s{};
    h5::write(0, "sessions", s);
}
