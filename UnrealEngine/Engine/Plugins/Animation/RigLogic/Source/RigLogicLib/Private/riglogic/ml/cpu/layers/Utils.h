// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

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

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
