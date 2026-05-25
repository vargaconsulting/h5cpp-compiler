#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[json::doc("Primitive type test"), json::alias("Primitives")]] Record {
    [[json::required]]
    char _char;
    unsigned char _uchar;
    short _short;
    [[json::format("int32")]]
    int _int;
    [[json::ignore]]
    long _long;
    float _float;
    [[json::required]]
    double _double;
    bool _bool;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/primitives", r);
}
