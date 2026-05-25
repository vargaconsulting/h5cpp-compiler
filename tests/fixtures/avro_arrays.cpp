#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[avro::doc("Array type test")]] Record {
    std::vector<int> ints;
    std::vector<double> doubles;
    std::vector<std::string> strings;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/arrays", r);
}
