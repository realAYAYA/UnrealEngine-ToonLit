// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/strategies/Block4.h"
#include "riglogic/system/simd/SIMD.h"

namespace rl4 {

namespace bpcm {

using ScalarJointCalculationStrategy = Block4JointCalculationStrategy<trimd::scalar::F128, float>;

}  // namespace bpcm

}  // namespace rl4
