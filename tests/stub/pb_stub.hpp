#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Minimal stub of the vargalabs/sandbox/pb.hpp public surface — enough for
// clang-tooling to resolve `pb::encode<T>(...)` / `pb::decode<T>(...)` calls
// in fixture sources. The real pb.hpp is header-only and ships in the sandbox
// repo; fixtures don't link against it.

namespace pb {
    using byte_t = std::uint8_t;
    using bytes_t = std::basic_string<byte_t>;

    template<class T> inline bytes_t encode(const T&)                        { return {}; }
    template<class T> inline void    encode_into(bytes_t&, const T&)         {}
    template<class T> inline T       decode(const byte_t*, std::size_t)      { return T{}; }
    template<class T> inline T       decode(const bytes_t&)                  { return T{}; }
}
