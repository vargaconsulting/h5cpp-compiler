#include "h5cpp_stub.hpp"

namespace sn {
struct Inner {
  int    a;
  double b;
};

struct Outer {
  int   idx;
  Inner inner_singleton;
  Inner inner_array[4];
  double field_02[3];
};
}

void use() {
  sn::Outer r;
  h5::write(0, "p/embedded-pod", r);
}
