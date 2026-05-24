#include "h5cpp_stub.hpp"
#include <map>

namespace sn::typecheck {
struct [[msgpack::doc("Map types test")]] Record {
    std::map<std::string, int> str_int_map;
    std::map<int, double> int_double_map;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/maps", r);
}
