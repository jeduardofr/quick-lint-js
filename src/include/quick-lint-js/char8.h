// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew Glazar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef QUICK_LINT_JS_CHAR8_H
#define QUICK_LINT_JS_CHAR8_H

#include <cstddef>
#include <iosfwd>
#include <quick-lint-js/char8.h>
#include <quick-lint-js/have.h>
#include <string>
#include <string_view>

namespace quick_lint_js {
#if QLJS_HAVE_CHAR8_T
using char8 = char8_t;
#else
using char8 = char;
#endif

// Alias std::u8string or std::string.
using string8 = std::basic_string<char8>;
using string8_view = std::basic_string_view<char8>;

#if QLJS_HAVE_CHAR8_T
class streamable_string8_view {
 public:
  friend std::ostream &operator<<(std::ostream &, streamable_string8_view);

 private:
  explicit streamable_string8_view(string8_view) noexcept;

  friend streamable_string8_view out_string8(string8_view) noexcept;

  string8_view sv_;
};

streamable_string8_view out_string8(string8_view) noexcept;
#else
inline string8_view out_string8(string8_view sv) noexcept { return sv; }
#endif

std::size_t strlen(const char8 *);
const char8 *strchr(const char8 *haystack, char8 needle);
const char8 *strstr(const char8 *haystack, const char8 *needle);

#if QLJS_HAVE_CHAR8_T
// Reimplementation of std::hash<string8_view>.
//
// libstdc++ 7.5.0 does not implement
// std::hash<std::basic_string_view<char8_t>>, so we provide a custom
// implementation.
struct string8_view_hash {
  std::size_t operator()(string8_view sv) const noexcept {
    std::hash<std::string_view> hasher;
    return hasher(
        std::string_view(reinterpret_cast<const char *>(sv.data()), sv.size()));
  }
};
#else
using string8_view_hash = std::hash<string8_view>;
#endif
}

namespace testing::internal {
#if QLJS_HAVE_CHAR8_T
void PrintTo(char8_t, std::ostream *);
void PrintTo(const char8_t *, std::ostream *);
#endif
}

#endif
