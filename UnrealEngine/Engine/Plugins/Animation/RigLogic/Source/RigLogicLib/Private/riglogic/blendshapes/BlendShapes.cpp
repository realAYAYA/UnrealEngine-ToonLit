// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/blendshapes/BlendShapes.h"

#include "riglogic/TypeDefs.h"

#include <cassert>
#include <cstddef>

namespace rl4 {

BlendShapes::BlendShapes(Vector<std::uint16_t>&& lods_,
                         Vector<std::uint16_t>&& inputIndices_,
                         Vector<std::uint16_t>&& outputIndices_) :
    lods{std::move(lods_)},
    inputIndices{std::move(inputIndices_)},
    outputIndices{std::move(outputIndices_)} {
}

void BlendShapes::calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const {
    assert(lod < lods.size());
    std::fill(outputs.begin(), outputs.end(), 0.0f);
    for (std::uint16_t i = 0u; i < lods[lod]; ++i) {
        outputs[outputIndices[i]] = inputs[inputIndices[i]];
    }
}

}  // namespace rl4
