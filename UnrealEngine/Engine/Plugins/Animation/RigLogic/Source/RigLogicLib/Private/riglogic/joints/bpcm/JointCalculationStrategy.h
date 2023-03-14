// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/JointStorageView.h"
#include "riglogic/types/Aliases.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

template<typename TValue>
struct JointCalculationStrategy {
    using JointGroupArrayView = ConstArrayView<JointGroupView<TValue> >;

    virtual ~JointCalculationStrategy() = default;

    virtual void calculate(const JointGroupArrayView& jointGroups,
                           ConstArrayView<float> inputs,
                           ArrayView<float> outputs,
                           std::uint16_t lod) const = 0;

    virtual void calculate(const JointGroupArrayView& jointGroups,
                           ConstArrayView<float> inputs,
                           ArrayView<float> outputs,
                           std::uint16_t lod,
                           std::uint16_t jointGroupIndex) const = 0;
};

}  // namespace bpcm

}  // namespace rl4
