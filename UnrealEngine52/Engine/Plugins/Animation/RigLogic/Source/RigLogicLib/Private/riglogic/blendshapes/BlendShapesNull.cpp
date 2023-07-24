// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/blendshapes/BlendShapesNull.h"

#include "riglogic/blendshapes/BlendShapesNullOutputInstance.h"

namespace rl4 {

BlendShapesOutputInstance::Pointer BlendShapesNull::createInstance(MemoryResource* instanceMemRes) const {
    return UniqueInstance<BlendShapesNullOutputInstance, BlendShapesOutputInstance>::with(instanceMemRes).create();
}

void BlendShapesNull::calculate(const ControlsInputInstance*  /*unused*/, BlendShapesOutputInstance*  /*unused*/,
                                std::uint16_t  /*unused*/) const {
}

void BlendShapesNull::load(terse::BinaryInputArchive<BoundedIOStream>&  /*unused*/) {
}

void BlendShapesNull::save(terse::BinaryOutputArchive<BoundedIOStream>&  /*unused*/) {
}

}  // namespace rl4
