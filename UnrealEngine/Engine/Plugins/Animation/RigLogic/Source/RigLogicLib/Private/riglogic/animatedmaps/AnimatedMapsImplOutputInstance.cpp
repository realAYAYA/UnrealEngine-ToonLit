// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/animatedmaps/AnimatedMapsImplOutputInstance.h"

#include "riglogic/TypeDefs.h"

#include <cstdint>

namespace rl4 {

AnimatedMapsImplOutputInstance::AnimatedMapsImplOutputInstance(std::uint16_t animatedMapCount, MemoryResource* memRes) :
    outputBuffer{animatedMapCount, {}, memRes} {
}

ArrayView<float> AnimatedMapsImplOutputInstance::getOutputBuffer() {
    return ArrayView<float>{outputBuffer};
}

}  // namespace rl4
