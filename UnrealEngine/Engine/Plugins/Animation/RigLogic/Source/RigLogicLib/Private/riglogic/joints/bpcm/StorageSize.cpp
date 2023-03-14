// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/StorageSize.h"

#include "riglogic/joints/bpcm/Extent.h"
#include "riglogic/utils/Extd.h"

#include <dna/layers/BehaviorReader.h>

#include <cassert>
#include <cstddef>

namespace rl4 {

namespace bpcm {

StorageSize::StorageSize(MemoryResource* memRes) :
    valueCount{},
    inputIndexCount{},
    outputIndexCount{},
    lodRegionCount{},
    lodCount{},
    jointGroups{memRes} {
}

void StorageSize::computeFrom(const dna::BehaviorReader* src, std::size_t padTo) {
    const auto jointGroupCount = src->getJointGroupCount();
    jointGroups.clear();
    jointGroups.reserve(jointGroupCount);
    lodCount = src->getLODCount();
    for (std::uint16_t i = 0u; i < jointGroupCount; ++i) {
        assert(src->getJointGroupLODs(i).size() == lodCount);
        const auto columnCount = src->getJointGroupInputIndices(i).size();
        const auto rowCount = src->getJointGroupOutputIndices(i).size();
        const std::size_t padding = extd::roundUp(rowCount, padTo) - rowCount;
        valueCount += (rowCount * columnCount) + (padding * columnCount);
        inputIndexCount += columnCount;
        outputIndexCount += (rowCount + padding);
        lodRegionCount += lodCount;
        jointGroups.push_back({
                    {static_cast<std::uint16_t>(rowCount), static_cast<std::uint16_t>(columnCount)},
                    {static_cast<std::uint16_t>(rowCount + padding), static_cast<std::uint16_t>(columnCount)}
                });
    }
}

JointGroupSize StorageSize::getJointGroupSize(std::size_t jointGroupIndex) const {
    assert(jointGroupIndex < jointGroups.size());
    return jointGroups[jointGroupIndex];
}

}  // namespace bpcm

}  // namespace rl4
