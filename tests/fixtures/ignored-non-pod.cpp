#include "h5cpp_stub.hpp"

#include <string>
#include <vector>

namespace sn {
struct Pod {
  int    idx;
  double value;
};

struct Container {
  double                      idx;
  std::string                 name;
  std::vector<Pod>            items;
};
}

void use() {
  sn::Pod r;
  h5::write(0, "p/pod", r);

  sn::Container c;
  h5::write(0, "p/container-should-be-skipped", c);
}
