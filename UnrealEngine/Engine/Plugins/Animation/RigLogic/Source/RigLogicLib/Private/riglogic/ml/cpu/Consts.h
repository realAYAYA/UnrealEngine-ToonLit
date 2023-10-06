// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include <cstdint>

namespace rl4 {

namespace ml {

namespace cpu {

constexpr std::uint32_t block4Width = 8u;
constexpr std::uint32_t block4Height = 4u;

constexpr std::uint32_t block8Width = 4u;
constexpr std::uint32_t block8Height = 8u;

constexpr std::uint32_t block16Width = 4u;
constexpr std::uint32_t block16Height = 16u;

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
