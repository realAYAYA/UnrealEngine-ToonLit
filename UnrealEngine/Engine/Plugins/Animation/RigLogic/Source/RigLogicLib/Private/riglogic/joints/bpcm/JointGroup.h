// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

namespace rl4 {

namespace bpcm {

struct JointGroup {
    // Start of non-zero values in storage
    std::uint32_t valuesOffset;
    // Start of sub-matrix col -> input vector mapping in storage
    std::uint32_t inputIndicesOffset;
    // Start of sub-matrix row -> output vector mapping in storage
    std::uint32_t outputIndicesOffset;
    // Start of LODs in storage
    std::uint32_t lodsOffset;
    // Sizes associated with start offsets
    std::uint32_t valuesSize;
    std::uint32_t inputIndicesSize;
    std::uint32_t inputIndicesSizeAlignedTo4;
    std::uint32_t inputIndicesSizeAlignedTo8;

    template<class Archive>
    void serialize(Archive& archive) {
        archive(valuesOffset,
                inputIndicesOffset,
                outputIndicesOffset,
                lodsOffset,
                valuesSize,
                inputIndicesSize,
                inputIndicesSizeAlignedTo4,
                inputIndicesSizeAlignedTo8);
    }

};

}  // namespace bpcm

}  // namespace rl4
