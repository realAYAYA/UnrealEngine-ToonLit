// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/blendshapes/BlendShapesNullOutputInstance.h"

#include <cstdint>

namespace rl4 {

ArrayView<float> BlendShapesNullOutputInstance::getOutputBuffer() {
    return {};
}

}  // namespace rl4
