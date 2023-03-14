// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

namespace rl4 {

namespace bpcm {

struct LODRegion {
    // Unaligned number of output indices
    std::uint32_t size;
    // If the last full block needs to be masked-off because of LODs,
    // this view will include that last full block as well
    std::uint32_t sizeAlignedToLastFullBlock;
    // If the last full block needs to be masked-off because of LODs,
    // that full block won't be included in this boundary as it needs special care
    std::uint32_t sizeAlignedToSecondLastFullBlock;

    LODRegion() = default;
    LODRegion(std::uint32_t lodRowCount, std::uint32_t rowCount, std::uint32_t blockHeight, std::uint32_t padTo);
    LODRegion(std::uint32_t lodRowCount,
              std::uint32_t lodRowCountAlignedToLastFullBlock,
              std::uint32_t lodRowCountAlignedToSecondLastFullBlock);

    template<class Archive>
    void serialize(Archive& archive) {
        archive(size, sizeAlignedToLastFullBlock, sizeAlignedToSecondLastFullBlock);
    }

};

}  // namespace bpcm

}  // namespace rl4
