// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <type_traits>

namespace rl4 {

namespace ml {

namespace cpu {

template<typename TFVec, std::size_t Size>
struct HasSize {
    static constexpr bool value = (TFVec::size() == Size);
};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4
