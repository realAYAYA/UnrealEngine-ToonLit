// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/blendshapes/BlendShapes.h"

#include <pma/utils/ManagedInstance.h>

namespace dna {

class BehaviorReader;

}  // namespace dna

namespace rl4 {

struct BlendShapesFactory {
    using ManagedBlendShapes = pma::UniqueInstance<BlendShapes>;
    using BlendShapesPtr = typename ManagedBlendShapes::PointerType;

    static BlendShapesPtr create(MemoryResource* memRes);
    static BlendShapesPtr create(const dna::BehaviorReader* reader, MemoryResource* memRes);
};

}  // namespace rl4
