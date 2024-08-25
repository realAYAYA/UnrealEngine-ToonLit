// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/strategies/Block4.h"
#include "riglogic/joints/bpcm/strategies/Block8.h"
#include "riglogic/system/simd/SIMD.h"

namespace rl4 {

namespace bpcm {

template<std::size_t BlockHeight>
struct Strategy;

template<>
struct Strategy<4> {
    template<typename T, typename TFVec>
    using Type = block4::Block4JointCalculationStrategy<T, TFVec>;
};

template<>
struct Strategy<8> {
    template<typename T, typename TFVec>
    using Type = block8::Block8JointCalculationStrategy<T, TFVec>;
};


}  // namespace bpcm

}  // namespace rl4
