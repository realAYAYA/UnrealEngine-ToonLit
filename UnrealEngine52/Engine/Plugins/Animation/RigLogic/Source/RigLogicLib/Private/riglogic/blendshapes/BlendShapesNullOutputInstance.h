// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/blendshapes/BlendShapesOutputInstance.h"

namespace rl4 {

class BlendShapesNullOutputInstance : public BlendShapesOutputInstance {
    public:
        ArrayView<float> getOutputBuffer() override;

};

}  // namespace rl4
