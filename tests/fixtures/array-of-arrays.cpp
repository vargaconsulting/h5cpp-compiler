#include "h5cpp_stub.hpp"

namespace sn {
struct Cell {
  float x;
  float y;
};

struct Grid {
  int  idx;
  Cell field_05[3][8];
};
}

void use() {
  sn::Grid g;
  h5::write(0, "p/array-of-arrays", g);
}
