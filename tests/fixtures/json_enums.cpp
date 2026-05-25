#include "h5cpp_stub.hpp"

namespace sn::typecheck {
enum class Color { Red, Green, Blue };

struct [[json::doc("Enum type test")]] Record {
    Color color;
    int id;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/enums", r);
}
