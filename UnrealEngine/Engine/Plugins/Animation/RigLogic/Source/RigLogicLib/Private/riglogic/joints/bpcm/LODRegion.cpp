// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/LODRegion.h"

#include "riglogic/utils/Extd.h"

#include <cstddef>

namespace rl4 {

namespace bpcm {

LODRegion::LODRegion(std::uint32_t lodRowCount, std::uint32_t rowCount, std::uint32_t blockHeight,
                     std::uint32_t padTo) : size{lodRowCount} {
    // Check whether the sub-matrix was padded to blockHeight size or padTo size
    const bool endsWithPadToBlock = (rowCount % blockHeight) == padTo;
    // Check if LOD boundary is within the region of the last padTo rows
    const bool isLODInLastPadToRows = (rowCount - lodRowCount) < padTo;
    // Check if LOD row is within the region of the last padTo-sized block
    const bool isLODInLastPadToBlock = (endsWithPadToBlock && isLODInLastPadToRows);
    // Round the LOD row to the nearest index that is modulo padTo or blockHeight
    // If LOD is in the last padTo-sized block, it rounds to padTo, otherwise to blockHeight
    const std::uint32_t alignTo = (isLODInLastPadToBlock ? padTo : blockHeight);
    const std::uint32_t upAlignedLodRow = extd::roundUp(lodRowCount, alignTo);
    // Check if the unaligned LOD boundary coincides with a block boundary
    // (e.g. this is the case for LOD-0)
    const bool isLODOnBlockBoundary = (upAlignedLodRow == lodRowCount);
    // Should the last blockHeight-sized block be masked-off or not
    // If LOD coincides with a block boundary, no mask-off is needed at all
    // If LOD is within the last padTo-sized block, no mask-off is needed for the last blockHeight-sized block
    const bool maskOffLastBlock = (!isLODOnBlockBoundary && !isLODInLastPadToBlock);
    // Find the last blockHeight-sized block boundary before the aligned LOD boundary
    // Sometimes this equals the aligned LOD boundary, but not if the matrix is padded to padTo block-size
    // and the LOD boundary is within that last padTo-sized block region
    sizeAlignedToLastFullBlock = upAlignedLodRow - (upAlignedLodRow % blockHeight);
    // This is one blockHeight before `sizeAlignedToLastFullBlock` because the last blockHeight-sized block
    // might need special care to mask off results that came from rows after the LOD boundary
    // If no mask-off is needed, it equals to `sizeAlignedToLastFullBlock`
    sizeAlignedToSecondLastFullBlock = (maskOffLastBlock ? sizeAlignedToLastFullBlock - blockHeight
                                        : sizeAlignedToLastFullBlock);
}

LODRegion::LODRegion(std::uint32_t lodRowCount,
                     std::uint32_t lodRowCountAlignedToLastFullBlock,
                     std::uint32_t lodRowCountAlignedToSecondLastFullBlock) :
    size{lodRowCount},
    sizeAlignedToLastFullBlock{lodRowCountAlignedToLastFullBlock},
    sizeAlignedToSecondLastFullBlock{lodRowCountAlignedToSecondLastFullBlock} {
}

}  // namespace bpcm

}  // namespace rl4
