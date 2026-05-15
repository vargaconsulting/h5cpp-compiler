#pragma once

#include <string>
#include <vector>

namespace h5 {
using hid_t = long;
using fd_t = long;

template <class T> hid_t write(hid_t, const std::string&, const T&)  { return 0; }
template <class T> hid_t read (hid_t, const std::string&)            { return 0; }
template <class T> hid_t create(hid_t, const std::string&)           { return 0; }
template <class T> hid_t append(hid_t, const std::string&, const T&) { return 0; }

template <class T> hid_t awrite(hid_t, const std::string&, const T&) { return 0; }
template <class T> hid_t aread (hid_t, const std::string&)           { return 0; }
template <class T> hid_t acreate(hid_t, const std::string&)          { return 0; }
}
