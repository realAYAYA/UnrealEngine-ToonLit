// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/blendshapes/BlendShapesOutputInstance.h"

#include <cstdint>

namespace rl4 {

class BlendShapesImplOutputInstance : public BlendShapesOutputInstance {
    public:
        BlendShapesImplOutputInstance(std::uint16_t blendShapeCount, MemoryResource* memRes);
        ArrayView<float> getOutputBuffer() override;

    private:
        Vector<float> outputBuffer;

};

}  // namespace rl4
