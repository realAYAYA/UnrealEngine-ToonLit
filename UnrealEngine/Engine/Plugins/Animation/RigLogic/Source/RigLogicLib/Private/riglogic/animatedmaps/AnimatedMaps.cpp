// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/animatedmaps/AnimatedMaps.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"

#include <cassert>
#include <utility>

namespace rl4 {

AnimatedMaps::AnimatedMaps(Vector<std::uint16_t>&& lods_, ConditionalTable&& conditionals_) :
    lods{std::move(lods_)},
    conditionals{std::move(conditionals_)} {
}

void AnimatedMaps::calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const {
    assert(lod < lods.size());
    conditionals.calculate(inputs.data(), outputs.data(), lods[lod]);
}

}  // namespace rl4
