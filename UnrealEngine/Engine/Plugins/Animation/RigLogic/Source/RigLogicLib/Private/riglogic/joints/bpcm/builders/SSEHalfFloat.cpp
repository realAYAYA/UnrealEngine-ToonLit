// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)

#include "riglogic/joints/bpcm/builders/SSEHalfFloat.h"

#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/SSE.h"
#include "riglogic/utils/Extd.h"

#include <dna/layers/BehaviorReader.h>
#include <pma/utils/ManagedInstance.h>

#include <immintrin.h>

#include <cstddef>
#include <cstdint>

namespace rl4 {

namespace bpcm {

inline void toHalfFloat128(const float* source, std::uint16_t* destination) {
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wold-style-cast"
        #if !defined(__clang__)
            #pragma GCC diagnostic ignored "-Wuseless-cast"
        #endif
    #endif
    __m128 block = _mm_load_ps(source);
    __m128i half = _mm_cvtps_ph(block, _MM_FROUND_CUR_DIRECTION);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(destination), half);
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
}

SSEJointsBuilder::~SSEJointsBuilder() = default;

SSEJointsBuilder::SSEJointsBuilder(MemoryResource* memRes_) : JointsBuilderCommon{block8Height, block4Height, memRes_} {
    strategy = pma::UniqueInstance<SSEJointCalculationStrategy<std::uint16_t>, CalculationStrategy>::with(memRes_).create();
}

void SSEJointsBuilder::setValues(const dna::BehaviorReader* reader) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < reader->getJointGroupCount(); ++i) {
        const auto source = reader->getJointGroupValues(i);
        const auto jointGroupSize = sizeReqs.jointGroups[i];
        storage.jointGroups[i].valuesOffset = offset;
        storage.jointGroups[i].valuesSize = jointGroupSize.padded.size();

        std::uint32_t remainder = jointGroupSize.original.rows % block8Height;
        std::uint32_t target = jointGroupSize.original.rows - remainder;
        // Copy the portion of the source matrix that is partitionable into block8Height chunks
        for (std::size_t row = 0ul; row < target; row += block8Height) {
            for (std::size_t col = 0ul; col < jointGroupSize.original.cols; ++col) {
                alignas(16) float fBlock[block8Height];
                alignas(16) std::uint16_t hfBlock[block8Height];

                for (std::size_t blkIdx = 0ul; blkIdx < block8Height; ++blkIdx) {
                    fBlock[blkIdx] = source[(row + blkIdx) * jointGroupSize.original.cols + col];
                }

                toHalfFloat128(fBlock, hfBlock);
                toHalfFloat128(fBlock + block4Height, hfBlock + block4Height);

                for (std::size_t blkIdx = 0ul; blkIdx < block8Height; ++blkIdx, ++offset) {
                    storage.values[offset] = hfBlock[blkIdx];
                }
            }
        }
        // Copy the non-block8Height sized remainder portion of the source matrix
        for (std::size_t row = target; row < jointGroupSize.original.rows; row += remainder) {
            for (std::size_t col = 0ul; col < jointGroupSize.original.cols; ++col) {
                alignas(16) float fBlock[block8Height] = {};
                alignas(16) std::uint16_t hfBlock[block8Height] = {};

                for (std::size_t blkIdx = 0ul; blkIdx < remainder; ++blkIdx) {
                    fBlock[blkIdx] = source[(row + blkIdx) * jointGroupSize.original.cols + col];
                }

                toHalfFloat128(fBlock, hfBlock);
                toHalfFloat128(fBlock + block4Height, hfBlock + block4Height);

                // Copy even the padded zero values as the representation of half-floats might
                // differ from the storage's zero-initialized state
                for (std::size_t blkIdx = 0ul; blkIdx < extd::roundUp(remainder, block4Height); ++blkIdx, ++offset) {
                    storage.values[offset] = hfBlock[blkIdx];
                }
            }
        }
    }
}

}  // namespace bpcm

}  // namespace rl4

#endif  // defined(RL_USE_HALF_FLOATS) && defined(RL_BUILD_WITH_SSE)
