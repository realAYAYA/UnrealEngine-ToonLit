// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/JointStorage.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

template<typename TValue>
struct JointGroupView {
    TValue* values;
    std::uint16_t* inputIndices;
    std::uint16_t* inputIndicesEnd;
    std::uint16_t* inputIndicesEndAlignedTo4;
    std::uint16_t* inputIndicesEndAlignedTo8;
    std::uint16_t* outputIndices;
    LODRegion* lods;
};

template<typename TValue>
Vector<JointGroupView<TValue> > takeStorageSnapshot(JointStorage<TValue>& storage, MemoryResource* memRes) {
    Vector<JointGroupView<TValue> > snapshot{storage.jointGroups.size(), {}, memRes};
    for (std::size_t i = 0ul; i < storage.jointGroups.size(); ++i) {
        const auto& jointGroup = storage.jointGroups[i];
        snapshot[i].values = storage.values.data() + jointGroup.valuesOffset;
        snapshot[i].inputIndices = storage.inputIndices.data() + jointGroup.inputIndicesOffset;
        snapshot[i].inputIndicesEnd = snapshot[i].inputIndices + jointGroup.inputIndicesSize;
        snapshot[i].inputIndicesEndAlignedTo4 = snapshot[i].inputIndices + jointGroup.inputIndicesSizeAlignedTo4;
        snapshot[i].inputIndicesEndAlignedTo8 = snapshot[i].inputIndices + jointGroup.inputIndicesSizeAlignedTo8;
        snapshot[i].outputIndices = storage.outputIndices.data() + jointGroup.outputIndicesOffset;
        snapshot[i].lods = storage.lodRegions.data() + jointGroup.lodsOffset;
    }
    return snapshot;
}

}  // namespace bpcm

}  // namespace rl4
