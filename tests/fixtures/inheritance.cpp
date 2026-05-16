#include "h5cpp_stub.hpp"

namespace sn {

// Base struct: satisfies isPOD() and will be reflected when referenced directly.
struct Base {
  int    id;
  double value;
};

// Derived struct: has a base class, so it is NOT POD (C++17 standard-layout
// rule §9/7 requires no base class for POD structs). The compiler's isPOD()
// guard skips it even though all its new fields are scalar.
// Only Base is reflected; Derived is ignored.
struct Derived : Base {
  float extra;
};

}

void use() {
  sn::Base b;
  h5::write(0, "p/base", b);

  sn::Derived d;
  h5::write(0, "p/derived-should-be-skipped", d);
}
