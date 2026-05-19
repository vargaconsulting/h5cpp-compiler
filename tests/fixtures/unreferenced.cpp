#include "h5cpp_stub.hpp"

namespace sn {
struct Referenced {
  int    idx;
  double value;
};

struct UnreferencedA {
  long  ignored_field_a;
  float ignored_field_b;
};

struct UnreferencedB {
  char  also_ignored;
};
}

void use() {
  sn::Referenced r;
  h5::write(0, "p/referenced", r);
}
