// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/animatedmaps/AnimatedMapsOutputInstance.h"

#include <cstdint>

namespace rl4 {

class AnimatedMapsImplOutputInstance : public AnimatedMapsOutputInstance {
    public:
        AnimatedMapsImplOutputInstance(std::uint16_t animatedMapCount, MemoryResource* memRes);
        ArrayView<float> getOutputBuffer() override;

    private:
        Vector<float> outputBuffer;

};

}  // namespace rl4
