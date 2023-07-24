// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

constexpr std::size_t cacheLineSize = 64ul;  // bytes
constexpr std::size_t lookAheadOffset = cacheLineSize / sizeof(float);  // in number of elements

constexpr std::uint32_t block4Width = 8u;
constexpr std::uint32_t block4Height = 4u;

constexpr std::uint32_t block8Width = 4u;
constexpr std::uint32_t block8Height = 8u;

constexpr std::uint32_t block16Width = 4u;
constexpr std::uint32_t block16Height = 16u;

}  // namespace bpcm

}  // namespace rl4
