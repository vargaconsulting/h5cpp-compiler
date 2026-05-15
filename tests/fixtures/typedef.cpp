#include "h5cpp_stub.hpp"

typedef unsigned long long MyUInt;
typedef double             MyDouble;

namespace sn {
struct Record {
  MyUInt   idx;
  MyUInt   aa;
  MyDouble value;
};
}

void use() {
  sn::Record r;
  h5::write(0, "p/typedef", r);
}
