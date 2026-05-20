#include "h5cpp_stub.hpp"

namespace sn::legacy {
struct Record {
  int    x;
  double y;
};
}

void use() {
  sn::legacy::Record r;
  h5::write(0, "legacy/record", r);
}
