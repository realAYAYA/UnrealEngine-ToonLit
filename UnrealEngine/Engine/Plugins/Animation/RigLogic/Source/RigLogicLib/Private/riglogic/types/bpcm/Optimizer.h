// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "riglogic/types/Extent.h"
#include "riglogic/utils/Extd.h"
#include "riglogic/utils/Macros.h"
#include "riglogic/system/simd/SIMD.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

template<typename TFVec, std::uint32_t BlockHeight, std::uint32_t PadTo>
struct Optimizer {

    static std::uint32_t optimize(float* dest, const float* source, Extent dimensions) {
        const std::uint32_t remainder = dimensions.rows % BlockHeight;
        const std::uint32_t target = dimensions.rows - remainder;
        std::uint32_t offset = 0u;
        // Copy the portion of the source matrix that is partitionable into BlockHeight chunks
        for (std::uint32_t row = 0u; row < target; row += BlockHeight) {
            for (std::uint32_t col = 0u; col < dimensions.cols; ++col) {
                for (std::uint32_t blkIdx = 0u; blkIdx < BlockHeight; ++blkIdx, ++offset) {
                    dest[offset] = source[(row + blkIdx) * dimensions.cols + col];
                }
            }
        }
        // Copy the non-BlockHeight sized remainder portion of the source matrix
        const std::uint32_t paddedBlockHeight = extd::roundUp(remainder, PadTo);
        for (std::uint32_t row = target; row < dimensions.rows; row += remainder) {
            for (std::uint32_t col = 0u; col < dimensions.cols; ++col) {
                for (std::uint32_t blkIdx = 0u; blkIdx < remainder; ++blkIdx, ++offset) {
                    dest[offset] = source[(row + blkIdx) * dimensions.cols + col];
                }
                // Skip over padding (already filled with zeros) in destination storage
                offset += (paddedBlockHeight - remainder);
            }
        }

        return offset;
    }

    static std::uint32_t optimize(std::uint16_t* dest, const float* source, Extent dimensions) {
        static constexpr std::size_t alignment = TFVec::alignment();
        const std::uint32_t remainder = dimensions.rows % BlockHeight;
        const std::uint32_t target = dimensions.rows - remainder;
        std::uint32_t offset = 0u;
        // Copy the portion of the source matrix that is partitionable into BlockHeight chunks
        for (std::uint32_t row = 0u; row < target; row += BlockHeight) {
            for (std::uint32_t col = 0u; col < dimensions.cols; ++col) {
                alignas(alignment) float input[BlockHeight];
                alignas(alignment) std::uint16_t output[BlockHeight];

                for (std::uint32_t blkIdx = 0u; blkIdx < BlockHeight; ++blkIdx) {
                    input[blkIdx] = source[(row + blkIdx) * dimensions.cols + col];
                }

                for (std::uint32_t blkOffset = 0u;
                     blkOffset < BlockHeight;
                     blkOffset += static_cast<std::uint32_t>(TFVec::size())) {
                    #if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 7)
                        TFVec::fromAlignedSource(input + blkOffset).unalignedStore(output + blkOffset);
                    #else
                        TFVec::fromAlignedSource(input + blkOffset).alignedStore(output + blkOffset);
                    #endif
                }

                for (std::uint32_t blkIdx = 0u; blkIdx < BlockHeight; ++blkIdx, ++offset) {
                    dest[offset] = output[blkIdx];
                }
            }
        }
        // Copy the non-BlockHeight sized remainder portion of the source matrix
        const std::uint32_t paddedBlockHeight = extd::roundUp(remainder, PadTo);
        for (std::uint32_t row = target; row < dimensions.rows; row += remainder) {
            for (std::uint32_t col = 0u; col < dimensions.cols; ++col) {
                alignas(alignment) float input[BlockHeight] = {};
                alignas(alignment) std::uint16_t output[BlockHeight] = {};

                for (std::uint32_t blkIdx = 0u; blkIdx < remainder; ++blkIdx) {
                    input[blkIdx] = source[(row + blkIdx) * dimensions.cols + col];
                }

                for (std::uint32_t blkOffset = 0u;
                     blkOffset < BlockHeight;
                     blkOffset += static_cast<std::uint32_t>(TFVec::size())) {
                    #if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 7)
                        TFVec::fromAlignedSource(input + blkOffset).unalignedStore(output + blkOffset);
                    #else
                        TFVec::fromAlignedSource(input + blkOffset).alignedStore(output + blkOffset);
                    #endif
                }

                // GCC-7 has a bug in its `array-bounds` check, and incorrectly warns about OOB access in the
                // below loop on `output[blkIdx]`, but `blkIdx` can never be higher than `paddedBlockHeight`,
                // which in turn is always either below or equal to `BlockHeight`, thus OOB is impossible.
                ASSUME_TRUE(paddedBlockHeight <= BlockHeight);
                // Copy even the padded zero values as the representation of half-floats might
                // differ from the storage's zero-initialized state
                for (std::uint32_t blkIdx = 0u; blkIdx < paddedBlockHeight; ++blkIdx, ++offset) {
                    dest[offset] = output[blkIdx];
                }
            }
        }

        return offset;
    }

};

}  // namespace bpcm

}  // namespace rl4
