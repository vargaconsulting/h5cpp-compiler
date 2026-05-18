#include "h5cpp_stub.hpp"

namespace sn {

enum class Color : int { Red = 0, Green = 1, Blue = 2 };

// Struct whose only field is an enum. The struct itself satisfies isPOD()
// (enum is a scalar type), so the compiler will enter the reflect path, but
// the enum field resolves to <unknown_type> inside utils::get_type_name.
// This exercises the type::invalid branch in utils::as<utils::type>().
struct Palette {
  Color primary;
  Color secondary;
};

}

void use() {
  sn::Palette p;
  h5::write(0, "p/enum-member", p);
}
