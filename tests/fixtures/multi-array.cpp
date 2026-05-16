#include "h5cpp_stub.hpp"

namespace sn {

// Struct with a multi-dimensional C array of a primitive type.
// Distinct from array-of-arrays (which has arrays of struct-typed members):
// here the element type is a plain float, exercising the array path where
// the inner element resolves to type::builtin rather than type::record.
struct Matrix {
  int   rows;
  int   cols;
  float data[4][4];
};

}

void use() {
  sn::Matrix m;
  h5::write(0, "p/multi-array", m);
}
