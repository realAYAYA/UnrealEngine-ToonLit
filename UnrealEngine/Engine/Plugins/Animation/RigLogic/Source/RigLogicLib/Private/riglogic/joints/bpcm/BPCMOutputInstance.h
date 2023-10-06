// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsOutputInstance.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

class OutputInstance : public JointsOutputInstance {
    public:
        OutputInstance(std::uint16_t jointAttributeCount, MemoryResource* memRes);

        ArrayView<float> getOutputBuffer() override;

    private:
        AlignedVector<float> outputBuffer;

};

}  // namespace bpcm

}  // namespace rl4
