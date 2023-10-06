// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/blendshapes/BlendShapesImplOutputInstance.h"

#include <cstdint>

namespace rl4 {

BlendShapesImplOutputInstance::BlendShapesImplOutputInstance(std::uint16_t blendShapeCount, MemoryResource* memRes) :
    outputBuffer{blendShapeCount, {}, memRes} {
}

ArrayView<float> BlendShapesImplOutputInstance::getOutputBuffer() {
    return ArrayView<float>{outputBuffer};
}

}  // namespace rl4
