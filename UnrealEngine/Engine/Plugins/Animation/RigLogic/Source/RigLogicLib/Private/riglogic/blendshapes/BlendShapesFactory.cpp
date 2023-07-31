// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/blendshapes/BlendShapesFactory.h"

#include "riglogic/utils/Extd.h"

#include <dna/layers/BehaviorReader.h>

#include <cstdint>
#include <utility>
#include <cstring>

namespace rl4 {

BlendShapesFactory::BlendShapesPtr BlendShapesFactory::create(MemoryResource* memRes) {
    return ManagedBlendShapes::with(memRes).create(Vector<std::uint16_t>{memRes},
                                                   Vector<std::uint16_t>{memRes},
                                                   Vector<std::uint16_t>{memRes});
}

BlendShapesFactory::BlendShapesPtr BlendShapesFactory::create(const dna::BehaviorReader* reader, MemoryResource* memRes) {
    Vector<std::uint16_t> lods{memRes};
    Vector<std::uint16_t> inputIndices{memRes};
    Vector<std::uint16_t> outputIndices{memRes};

    extd::copy(reader->getBlendShapeChannelLODs(), lods);
    extd::copy(reader->getBlendShapeChannelInputIndices(), inputIndices);
    extd::copy(reader->getBlendShapeChannelOutputIndices(), outputIndices);

    return ManagedBlendShapes::with(memRes).create(std::move(lods),
                                                   std::move(inputIndices),
                                                   std::move(outputIndices));
}

}  // namespace rl4
