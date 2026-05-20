#include "h5cpp_stub.hpp"

struct Record {
  int a;
  double b;
};

void use() {
  Record r1, r2;
  h5::write(0, "path/one", r1);
  h5::write(0, "path/two", r2);
}
