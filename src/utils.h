#pragma once

#include <string>
#include <string_view>
#include <span>
#include <format>
#include <sstream>

namespace utils {

    template <typename T>
    std::string join(std::span<T> elements, std::string_view separator = ", ") {
        std::string joined;
        for (size_t i = 0; i < elements.size(); ++i) {
            joined += std::format("{}", elements[i]);
            if (i + 1 < elements.size())
                joined += separator;
        }
        return joined;
    }

    inline std::string join(const char* const* array, uint32_t count, const std::string& delimiter = ", ") {
        std::ostringstream oss;
        for (uint32_t i = 0; i < count; ++i) {
            if (i != 0) {
                oss << delimiter;
            }
            oss << array[i];
        }
        return oss.str();
    }

    template <typename T, size_t N>
    std::string join(const T(&elements)[N], std::string_view separator = ", ") {
        return join(std::span<const T>(elements), separator);
    }

    template<class T>
    std::string fmt_ptr(T* p) {
        std::ostringstream o; o << (const void*)p; return o.str();
    }

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

} // namespace utils
