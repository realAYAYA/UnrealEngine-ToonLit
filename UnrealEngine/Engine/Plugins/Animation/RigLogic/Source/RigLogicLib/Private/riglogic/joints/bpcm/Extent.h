// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

namespace rl4 {

namespace bpcm {

struct Extent {
    std::uint16_t rows;
    std::uint16_t cols;

    std::uint16_t size() const {
        return static_cast<std::uint16_t>(rows * cols);
    }

};

inline bool operator==(const Extent& lhs, const Extent& rhs) {
    return (lhs.rows == rhs.rows && lhs.cols == rhs.cols);
}

inline bool operator!=(const Extent& lhs, const Extent& rhs) {
    return !(lhs == rhs);
}

}  // namespace bpcm

}  // namespace rl4
