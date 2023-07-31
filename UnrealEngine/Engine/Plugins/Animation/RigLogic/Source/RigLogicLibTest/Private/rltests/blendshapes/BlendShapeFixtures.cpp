// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/blendshapes/BlendShapeFixtures.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/blendshapes/BlendShapes.h"

namespace rltests {

rl4::BlendShapes createTestBlendShapes(rl4::MemoryResource* memRes) {
    return rl4::BlendShapes{
        rl4::Vector<std::uint16_t>{std::begin(LODs), std::end(LODs), memRes},
        rl4::Vector<std::uint16_t>{std::begin(blendShapeInputIndices), std::end(blendShapeInputIndices), memRes},
        rl4::Vector<std::uint16_t>{std::begin(blendShapeOutputIndices), std::end(blendShapeOutputIndices), memRes}
    };
}

}  // namespace rltests
