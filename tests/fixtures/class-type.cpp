#include "h5cpp_stub.hpp"

class Particle {
public:
  double x, y, z;
  int id;
};

void use() {
  Particle p;
  h5::write(0, "class/particle", p);
}
