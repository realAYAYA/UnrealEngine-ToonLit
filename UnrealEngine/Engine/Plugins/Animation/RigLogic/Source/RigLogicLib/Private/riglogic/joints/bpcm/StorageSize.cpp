// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/StorageSize.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/types/Extent.h"
#include "riglogic/utils/Extd.h"

#include <cassert>
#include <cstddef>

namespace rl4 {

namespace bpcm {

StorageSize::StorageSize(MemoryResource* memRes) :
    attributeCount{},
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
    attributeCount = src->getJointRowCount();
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
        #if !defined(__clang__) && defined(__GNUC__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wuseless-cast"
        #endif
        jointGroups.push_back({
                    {static_cast<std::uint32_t>(rowCount), static_cast<std::uint32_t>(columnCount)},
                    {static_cast<std::uint32_t>(rowCount + padding), static_cast<std::uint32_t>(columnCount)}
                });
        #if !defined(__clang__) && defined(__GNUC__)
            #pragma GCC diagnostic pop
        #endif
    }
}

JointGroupSize StorageSize::getJointGroupSize(std::size_t jointGroupIndex) const {
    assert(jointGroupIndex < jointGroups.size());
    return jointGroups[jointGroupIndex];
}

}  // namespace bpcm

}  // namespace rl4
