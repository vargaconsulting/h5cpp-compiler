#include "h5cpp_stub.hpp"

namespace sn {

struct Leaf {
  int x;
  int y;
};

struct LeafSibling {
  float a;
  float b;
};

struct Branch {
  int  branch_idx;
  Leaf leaves[4];
};

struct Root {
  int         root_idx;
  Branch      branches[3];
  LeafSibling extras[2];
};

}

void use() {
  sn::Root r;
  h5::write(0, "p/topological", r);
}
