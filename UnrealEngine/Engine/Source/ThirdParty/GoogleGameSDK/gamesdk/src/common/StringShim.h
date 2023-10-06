/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

// These string functions can be needed when compiling using GNU STL.

#if (defined ANDROID_GNUSTL) || \
    ((defined ANDROID_NDK_VERSION) && ANDROID_NDK_VERSION <= 17)

namespace std {

template <typename T>
std::string to_string(T value) {
    std::stringstream os;
    os << value;
    return os.str();
}
template <typename T>
std::wstring to_wstring(T value) {
    std::wstringstream os;
    os << value;
    return os.str();
}

}  // namespace std

#endif

#if (defined ANDROID_GNUSTL)

namespace std {

long double stold(const std::string& str, std::size_t* pos = nullptr) {
    long double d;
    std::stringstream is(str);
    auto p0 = is.tellg();
    is >> d;
    if (pos != nullptr) {
        *pos = is.tellg() - p0;
    }
    return d;
}

}  // namespace std
#endif
