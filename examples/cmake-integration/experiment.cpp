#include <vector>

// Minimal stub so h5cpp-compiler can match the h5::write template argument.
namespace h5 {
    template <typename T>
    void write(void*, const char*, const std::vector<T>&) {}
}

struct Particle {
    double x, y, z;
    int id;
};

int main() {
    std::vector<Particle> particles(100);
    h5::write(nullptr, "particles", particles);
    return 0;
}
