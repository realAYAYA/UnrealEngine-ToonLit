// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/BPCMOutputInstance.h"

#include "riglogic/TypeDefs.h"

#include <cstdint>
#include <cstddef>

namespace rl4 {

namespace bpcm {

OutputInstance::OutputInstance(std::uint16_t jointAttributeCount, MemoryResource* memRes) :
    outputBuffer{jointAttributeCount, {}, memRes} {
}

ArrayView<float> OutputInstance::getOutputBuffer() {
    return ArrayView<float>{outputBuffer};
}

}  // namespace bpcm

}  // namespace rl4
