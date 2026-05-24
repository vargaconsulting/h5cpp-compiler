#include "h5cpp_stub.hpp"

namespace sn::typecheck {
struct [[cbor::alias("Primitives"), cbor::doc("Primitive type test")]] Record {
    [[cbor::required]]
    char _char;
    unsigned char _uchar;
    short _short;
    unsigned short _ushort;
    int _int;
    unsigned int _uint;
    long _long;
    unsigned long _ulong;
    long long _llong;
    unsigned long long _ullong;
    float _float;
    double _double;
    bool _bool;
};
}

void use() {
    sn::typecheck::Record r;
    h5::write(0, "p/primitives", r);
}
