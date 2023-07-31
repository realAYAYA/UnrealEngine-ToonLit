// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/Extent.h"

#include <cstddef>

namespace rl4 {

namespace bpcm {

struct JointGroupSize {
    Extent original;
    Extent padded;
};

struct StorageSize {
    std::size_t valueCount;
    std::size_t inputIndexCount;
    std::size_t outputIndexCount;
    std::size_t lodRegionCount;
    std::size_t lodCount;
    Vector<JointGroupSize> jointGroups;

    explicit StorageSize(MemoryResource* memRes);

    void computeFrom(const dna::BehaviorReader* src, std::size_t padTo);
    JointGroupSize getJointGroupSize(std::size_t jointGroupIndex) const;
};

}  // namespace bpcm

}  // namespace rl4
