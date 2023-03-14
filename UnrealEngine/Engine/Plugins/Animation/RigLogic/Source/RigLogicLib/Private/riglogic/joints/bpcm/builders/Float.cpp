// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/builders/Float.h"

#include "riglogic/joints/bpcm/Consts.h"

#include <dna/layers/BehaviorReader.h>

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

FloatStorageBuilder::FloatStorageBuilder(std::uint32_t blockHeight_, std::uint32_t padTo_,
                                         MemoryResource* memRes_) : JointsBuilderCommon{blockHeight_, padTo_, memRes_} {
}

FloatStorageBuilder::~FloatStorageBuilder() = default;

void FloatStorageBuilder::setValues(const dna::BehaviorReader* reader) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < reader->getJointGroupCount(); ++i) {
        const auto source = reader->getJointGroupValues(i);
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        storage.jointGroups[i].valuesOffset = offset;
        storage.jointGroups[i].valuesSize = jointGroupSize.padded.size();
        const std::uint32_t remainder = jointGroupSize.original.rows % blockHeight;
        const std::uint32_t target = jointGroupSize.original.rows - remainder;
        // Copy the portion of the source matrix that is partitionable into blockHeight chunks
        for (std::size_t row = 0ul; row < target; row += blockHeight) {
            for (std::size_t col = 0ul; col < jointGroupSize.original.cols; ++col) {
                for (std::size_t blkIdx = 0ul; blkIdx < blockHeight; ++blkIdx, ++offset) {
                    storage.values[offset] = source[(row + blkIdx) * jointGroupSize.original.cols + col];
                }
            }
        }
        // Copy the non-blockHeight sized remainder portion of the source matrix
        for (std::size_t row = target; row < jointGroupSize.original.rows; row += remainder) {
            for (std::size_t col = 0ul; col < jointGroupSize.original.cols; ++col) {
                for (std::size_t blkIdx = 0ul; blkIdx < remainder; ++blkIdx, ++offset) {
                    storage.values[offset] = source[(row + blkIdx) * jointGroupSize.original.cols + col];
                }
                // Skip over padding (already filled with zeros) in destination storage
                offset += static_cast<std::uint32_t>(jointGroupSize.padded.rows - jointGroupSize.original.rows);
            }
        }
    }
}

}  // namespace bpcm

}  // namespace rl4
