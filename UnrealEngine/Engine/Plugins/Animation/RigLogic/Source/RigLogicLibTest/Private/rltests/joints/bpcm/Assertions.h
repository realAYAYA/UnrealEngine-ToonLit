// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "rltests/Defs.h"

#include "riglogic/joints/bpcm/JointsEvaluator.h"

namespace rl4 {

namespace bpcm {

template<typename TValue>
struct Evaluator<TValue>::Accessor {

    static void assertRawDataEqual(const Evaluator<TValue>& result, const Evaluator<TValue>& expected) {
        ASSERT_EQ(result.storage.inputIndices, expected.storage.inputIndices);
        ASSERT_EQ(result.storage.outputIndices, expected.storage.outputIndices);
        ASSERT_EQ(result.storage.values, expected.storage.values);
    }

    static void assertJointGroupsEqual(const Evaluator<TValue>& result, const Evaluator<TValue>& expected) {
        ASSERT_EQ(result.storage.jointGroups.size(), expected.storage.jointGroups.size());
        for (std::size_t jgIdx = 0ul; jgIdx < expected.storage.jointGroups.size(); ++jgIdx) {
            const auto& jointGroup = result.storage.jointGroups[jgIdx];
            const auto& expectedJointGroup = expected.storage.jointGroups[jgIdx];
            ASSERT_EQ(jointGroup.inputIndicesOffset, expectedJointGroup.inputIndicesOffset);
            ASSERT_EQ(jointGroup.inputIndicesSize, expectedJointGroup.inputIndicesSize);
            ASSERT_EQ(jointGroup.inputIndicesSizeAlignedTo4, expectedJointGroup.inputIndicesSizeAlignedTo4);
            ASSERT_EQ(jointGroup.inputIndicesSizeAlignedTo8, expectedJointGroup.inputIndicesSizeAlignedTo8);
            ASSERT_EQ(jointGroup.lodsOffset, expectedJointGroup.lodsOffset);
            ASSERT_EQ(jointGroup.outputIndicesOffset, expectedJointGroup.outputIndicesOffset);
            ASSERT_EQ(jointGroup.valuesOffset, expectedJointGroup.valuesOffset);
            ASSERT_EQ(jointGroup.valuesSize, expectedJointGroup.valuesSize);
        }
    }

    static void assertLODsEqual(const Evaluator<TValue>& result, const Evaluator<TValue>& expected) {
        ASSERT_EQ(result.storage.lodRegions.size(), expected.storage.lodRegions.size());
        for (std::size_t lod = 0; lod < expected.storage.lodRegions.size(); ++lod) {
            const auto& lodRegion = result.storage.lodRegions[lod];
            const auto& expectedLodRegion = expected.storage.lodRegions[lod];
            ASSERT_EQ(lodRegion.size, expectedLodRegion.size);
            ASSERT_EQ(lodRegion.sizeAlignedToLastFullBlock, expectedLodRegion.sizeAlignedToLastFullBlock);
            ASSERT_EQ(lodRegion.sizeAlignedToSecondLastFullBlock,
                      expectedLodRegion.sizeAlignedToSecondLastFullBlock);
        }
    }

};

}  // namespace bpcm

}  // namespace rl4
