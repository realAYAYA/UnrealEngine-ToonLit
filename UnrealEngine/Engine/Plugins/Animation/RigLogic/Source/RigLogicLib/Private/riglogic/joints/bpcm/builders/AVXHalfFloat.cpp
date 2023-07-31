// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)

#include "riglogic/joints/bpcm/builders/AVXHalfFloat.h"

#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/AVX.h"
#include "riglogic/utils/Extd.h"

#include <dna/layers/BehaviorReader.h>
#include <pma/utils/ManagedInstance.h>

#include <immintrin.h>

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

inline void toHalfFloat256(const float* source, std::uint16_t* destination) {
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wold-style-cast"
        #if !defined(__clang__)
            #pragma GCC diagnostic ignored "-Wuseless-cast"
        #endif
    #endif
    __m256 block = _mm256_load_ps(source);
    __m128i half = _mm256_cvtps_ph(block, _MM_FROUND_CUR_DIRECTION);
    _mm_store_si128(reinterpret_cast<__m128i*>(destination), half);
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
}

AVXJointsBuilder::AVXJointsBuilder(MemoryResource* memRes_) : JointsBuilderCommon{block16Height, block8Height, memRes_} {
    strategy = pma::UniqueInstance<AVXJointCalculationStrategy<std::uint16_t>, CalculationStrategy>::with(memRes_).create();
}

AVXJointsBuilder::~AVXJointsBuilder() = default;

void AVXJointsBuilder::setValues(const dna::BehaviorReader* reader) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < reader->getJointGroupCount(); ++i) {
        const auto source = reader->getJointGroupValues(i);
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        storage.jointGroups[i].valuesOffset = offset;
        storage.jointGroups[i].valuesSize = jointGroupSize.padded.size();

        std::uint32_t remainder = jointGroupSize.original.rows % block16Height;
        std::uint32_t target = jointGroupSize.original.rows - remainder;
        // Copy the portion of the source matrix that is partitionable into block16Height chunks
        for (std::size_t row = 0ul; row < target; row += block16Height) {
            for (std::size_t col = 0ul; col < jointGroupSize.original.cols; ++col) {
                alignas(32) float fBlock[block16Height];
                alignas(16) std::uint16_t hfBlock[block16Height];

                for (std::size_t blkIdx = 0ul; blkIdx < block16Height; ++blkIdx) {
                    fBlock[blkIdx] = source[(row + blkIdx) * jointGroupSize.original.cols + col];
                }

                toHalfFloat256(fBlock, hfBlock);
                toHalfFloat256(fBlock + block8Height, hfBlock + block8Height);

                for (std::size_t blkIdx = 0ul; blkIdx < block16Height; ++blkIdx, ++offset) {
                    storage.values[offset] = hfBlock[blkIdx];
                }
            }
        }
        // Copy the non-blockHeight sized remainder portion of the source matrix
        for (std::size_t row = target; row < jointGroupSize.original.rows; row += remainder) {
            for (std::size_t col = 0ul; col < jointGroupSize.original.cols; ++col) {
                alignas(32) float fBlock[block16Height] = {};
                alignas(16) std::uint16_t hfBlock[block16Height] = {};

                for (std::size_t blkIdx = 0ul; blkIdx < remainder; ++blkIdx) {
                    fBlock[blkIdx] = source[(row + blkIdx) * jointGroupSize.original.cols + col];
                }

                toHalfFloat256(fBlock, hfBlock);
                toHalfFloat256(fBlock + block8Height, hfBlock + block8Height);

                // Copy even the padded zero values as the representation of half-floats might
                // differ from the storage's zero-initialized state
                for (std::size_t blkIdx = 0ul; blkIdx < extd::roundUp(remainder, block8Height); ++blkIdx, ++offset) {
                    storage.values[offset] = hfBlock[blkIdx];
                }
            }
        }
    }
}

}  // namespace bpcm

}  // namespace rl4

#endif  // defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_AVX)
