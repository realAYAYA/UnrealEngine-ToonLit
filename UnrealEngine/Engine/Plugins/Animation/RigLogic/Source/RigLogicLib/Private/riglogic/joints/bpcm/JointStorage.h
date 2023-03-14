// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/JointGroup.h"
#include "riglogic/joints/bpcm/LODRegion.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

template<typename TValue>
struct JointStorage {
    // All non-zero values
    AlignedVector<TValue> values;
    // Sub-matrix col -> input vector
    AlignedVector<std::uint16_t> inputIndices;
    // Sub-matrix row -> output vector
    AlignedVector<std::uint16_t> outputIndices;
    // Output index boundaries for each LOD
    Vector<LODRegion> lodRegions;
    // Delineate storage into joint-groups
    Vector<JointGroup> jointGroups;

    explicit JointStorage(MemoryResource* memRes) :
        values{memRes},
        inputIndices{memRes},
        outputIndices{memRes},
        lodRegions{memRes},
        jointGroups{memRes} {
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(values, inputIndices, outputIndices, lodRegions, jointGroups);
    }

};

}  // namespace bpcm

}  // namespace rl4
