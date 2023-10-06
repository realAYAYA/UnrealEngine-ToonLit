// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/strategies/Block8.h"
#include "riglogic/system/simd/SIMD.h"

namespace rl4 {

namespace bpcm {

template<typename T>
using AVXJointCalculationStrategy = Block8JointCalculationStrategy<trimd::avx::F256, T>;

}  // namespace bpcm

}  // namespace rl4
