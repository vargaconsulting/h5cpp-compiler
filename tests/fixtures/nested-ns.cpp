#include "h5cpp_stub.hpp"

namespace outer {
namespace middle {
namespace inner {
struct Record {
  int    idx;
  double value;
};
}
}
}

void use() {
  outer::middle::inner::Record r;
  h5::write(0, "p/nested-ns", r);
}
