// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/JointCalculationStrategy.h"
#include "riglogic/joints/bpcm/JointGroup.h"
#include "riglogic/joints/bpcm/JointStorageView.h"
#include "riglogic/joints/bpcm/LODRegion.h"
#include "riglogic/utils/Macros.h"

#include <cstddef>
#include <cstdint>

// The Block-8 algorithm follows the same logic and data layout as the Block-4 algorithm,
// and the only difference between the two is the size of blocks that are processed.

namespace rl4 {

namespace bpcm {

template<typename TF256, typename T>
static FORCE_INLINE void processBlocks16x1(const std::uint16_t* inputIndicesEndAlignedTo4,
                                           const std::uint16_t* inputIndicesEnd,
                                           ConstArrayView<float> inputs,
                                           const T*& values,
                                           TF256& sum1,
                                           TF256& sum2) {
    for (const std::uint16_t* inputIndices = inputIndicesEndAlignedTo4;
         inputIndices < inputIndicesEnd;
         ++inputIndices, values += (2ul * TF256::size())) {
        const TF256 inputVec{inputs[*inputIndices]};
        const TF256 blk1 = TF256::fromAlignedSource(values);
        const TF256 blk2 = TF256::fromAlignedSource(values + TF256::size());
        sum1 += (blk1 * inputVec);
        sum2 += (blk2 * inputVec);
    }
}

template<typename TF256, typename T>
static FORCE_INLINE void processBlocks16x4(const std::uint16_t* inputIndicesStart,
                                           const std::uint16_t* inputIndicesEndAlignedTo4,
                                           const std::uint16_t* inputIndicesEnd,
                                           ConstArrayView<float> inputs,
                                           const T*& values,
                                           float* outbuf) {
    TF256 sum1{};
    TF256 sum2{};
    TF256 sum3{};
    TF256 sum4{};
    TF256 sum5{};
    TF256 sum6{};
    TF256 sum7{};
    TF256 sum8{};
    for (const std::uint16_t* inputIndices = inputIndicesStart;
         inputIndices < inputIndicesEndAlignedTo4;
         inputIndices += block16Width, values += (2ul * TF256::size() * block16Width)) {
        const TF256 inputVec1{inputs[inputIndices[0]]};
        const TF256 inputVec2{inputs[inputIndices[1]]};
        const TF256 inputVec3{inputs[inputIndices[2]]};
        const TF256 inputVec4{inputs[inputIndices[3]]};
        const TF256 blk1 = TF256::fromAlignedSource(values);
        const TF256 blk2 = TF256::fromAlignedSource(values + TF256::size());
        const TF256 blk3 = TF256::fromAlignedSource(values + TF256::size() * 2);
        const TF256 blk4 = TF256::fromAlignedSource(values + TF256::size() * 3);
        const TF256 blk5 = TF256::fromAlignedSource(values + TF256::size() * 4);
        const TF256 blk6 = TF256::fromAlignedSource(values + TF256::size() * 5);
        const TF256 blk7 = TF256::fromAlignedSource(values + TF256::size() * 6);
        const TF256 blk8 = TF256::fromAlignedSource(values + TF256::size() * 7);
        #ifndef RL_NO_MANUAL_PREFETCH
            #ifdef RL_USE_NON_TEMPORAL_PREFETCH
                TF256::prefetchNTA(values + (lookAheadOffset * 6ul));
                TF256::prefetchNTA(values + (lookAheadOffset * 7ul));
                TF256::prefetchNTA(values + (lookAheadOffset * 8ul));
                TF256::prefetchNTA(values + (lookAheadOffset * 9ul));
                TF256::prefetchNTA(values + (lookAheadOffset * 10ul));
                TF256::prefetchNTA(values + (lookAheadOffset * 11ul));
            #else
                TF256::prefetchT0(values + (lookAheadOffset * 6ul));
                TF256::prefetchT0(values + (lookAheadOffset * 7ul));
                TF256::prefetchT0(values + (lookAheadOffset * 8ul));
                TF256::prefetchT0(values + (lookAheadOffset * 9ul));
                TF256::prefetchT0(values + (lookAheadOffset * 10ul));
                TF256::prefetchT0(values + (lookAheadOffset * 11ul));
            #endif
        #endif
        sum1 += (blk1 * inputVec1);
        sum2 += (blk2 * inputVec1);
        sum3 += (blk3 * inputVec2);
        sum4 += (blk4 * inputVec2);
        sum5 += (blk5 * inputVec3);
        sum6 += (blk6 * inputVec3);
        sum7 += (blk7 * inputVec4);
        sum8 += (blk8 * inputVec4);
    }
    // Process 16x1 horizontal remainder portion after 16x4 blocks are consumed
    processBlocks16x1(inputIndicesEndAlignedTo4, inputIndicesEnd, inputs, values, sum1, sum2);

    sum1 += sum3;
    sum2 += sum4;
    sum5 += sum7;
    sum6 += sum8;
    sum1 += sum5;
    sum2 += sum6;

    sum1.alignedStore(outbuf);
    sum2.alignedStore(outbuf + TF256::size());
}

template<typename TF256, typename T>
static FORCE_INLINE void processBlocks8x1(const std::uint16_t* inputIndicesEndAlignedTo8,
                                          const std::uint16_t* inputIndicesEnd,
                                          ConstArrayView<float> inputs,
                                          const T*& values,
                                          TF256& sum1) {
    for (const std::uint16_t* inputIndices = inputIndicesEndAlignedTo8;
         inputIndices < inputIndicesEnd;
         ++inputIndices, values += TF256::size()) {
        const TF256 inputVec{inputs[*inputIndices]};
        const TF256 blk = TF256::fromAlignedSource(values);
        sum1 += (blk * inputVec);
    }
}

template<typename TF256, typename T>
static FORCE_INLINE void processBlocks8x8(const std::uint16_t* inputIndicesStart,
                                          const std::uint16_t* inputIndicesEndAlignedTo8,
                                          const std::uint16_t* inputIndicesEnd,
                                          ConstArrayView<float> inputs,
                                          const T*& values,
                                          float* outbuf) {
    TF256 sum1{};
    TF256 sum2{};
    TF256 sum3{};
    TF256 sum4{};
    TF256 sum5{};
    TF256 sum6{};
    TF256 sum7{};
    TF256 sum8{};
    for (const std::uint16_t* inputIndices = inputIndicesStart;
         inputIndices < inputIndicesEndAlignedTo8;
         inputIndices += block4Width, values += (TF256::size() * block4Width)) {
        const TF256 inputVec1{inputs[inputIndices[0]]};
        const TF256 inputVec2{inputs[inputIndices[1]]};
        const TF256 inputVec3{inputs[inputIndices[2]]};
        const TF256 inputVec4{inputs[inputIndices[3]]};
        const TF256 inputVec5{inputs[inputIndices[4]]};
        const TF256 inputVec6{inputs[inputIndices[5]]};
        const TF256 inputVec7{inputs[inputIndices[6]]};
        const TF256 inputVec8{inputs[inputIndices[7]]};
        const TF256 blk1 = TF256::fromAlignedSource(values);
        const TF256 blk2 = TF256::fromAlignedSource(values + TF256::size());
        const TF256 blk3 = TF256::fromAlignedSource(values + TF256::size() * 2);
        const TF256 blk4 = TF256::fromAlignedSource(values + TF256::size() * 3);
        const TF256 blk5 = TF256::fromAlignedSource(values + TF256::size() * 4);
        const TF256 blk6 = TF256::fromAlignedSource(values + TF256::size() * 5);
        const TF256 blk7 = TF256::fromAlignedSource(values + TF256::size() * 6);
        const TF256 blk8 = TF256::fromAlignedSource(values + TF256::size() * 7);
        sum1 += (blk1 * inputVec1);
        sum2 += (blk2 * inputVec2);
        sum3 += (blk3 * inputVec3);
        sum4 += (blk4 * inputVec4);
        sum5 += (blk5 * inputVec5);
        sum6 += (blk6 * inputVec6);
        sum7 += (blk7 * inputVec7);
        sum8 += (blk8 * inputVec8);
    }
    // Process 8x1 horizontal remainder portion after 8x8 blocks are consumed
    processBlocks8x1(inputIndicesEndAlignedTo8, inputIndicesEnd, inputs, values, sum1);

    sum1 += sum2;
    sum3 += sum4;
    sum5 += sum6;
    sum7 += sum8;
    sum1 += sum3;
    sum5 += sum7;
    sum1 += sum5;

    sum1.alignedStore(outbuf);
}

/*
 * Orchestrate the execution of the needed block processors for a given joint group
 */
template<typename TF256, typename T>
static FORCE_INLINE void processJointGroupBlock8(const JointGroupView<T>& jointGroup, ConstArrayView<float> inputs,
                                                 ArrayView<float> outputs, std::uint16_t lod) {
    const T* values = jointGroup.values;
    const std::uint16_t* const inputIndices = jointGroup.inputIndices;
    const std::uint16_t* const inputIndicesEnd = jointGroup.inputIndicesEnd;
    const std::uint16_t* const inputIndicesEndAlignedTo4 = jointGroup.inputIndicesEndAlignedTo4;
    const std::uint16_t* const inputIndicesEndAlignedTo8 = jointGroup.inputIndicesEndAlignedTo8;
    const LODRegion& lodRegion = jointGroup.lods[lod];
    const std::uint16_t* outputIndices = jointGroup.outputIndices;
    const std::uint16_t* const outputIndicesEnd = outputIndices + lodRegion.size;
    const std::uint16_t* const outputIndicesEndAlignedToLastFullBlock = outputIndices + lodRegion.sizeAlignedToLastFullBlock;
    const std::uint16_t* const outputIndicesEndAlignedToSecondLastFullBlock = outputIndices +
        lodRegion.sizeAlignedToSecondLastFullBlock;
    // Process portion of matrix that's partitionable into 16x4 blocks
    for (; outputIndices < outputIndicesEndAlignedToSecondLastFullBlock; outputIndices += block16Height) {
        alignas(TF256::alignment()) float outbuf[block16Height];
        processBlocks16x4<TF256>(inputIndices, inputIndicesEndAlignedTo4, inputIndicesEnd, inputs, values,
                                 static_cast<float*>(outbuf));
        for (std::size_t i = 0ul; i < block16Height; ++i) {
            outputs[outputIndices[i]] = outbuf[i];
        }
    }
    // Process the last 16x4 block which needs special handling when some of
    // the output values need to be masked-off because of LODs
    for (; outputIndices < outputIndicesEndAlignedToLastFullBlock; outputIndices += block16Height) {
        alignas(TF256::alignment()) float outbuf[block16Height];
        processBlocks16x4<TF256>(inputIndices, inputIndicesEndAlignedTo4, inputIndicesEnd, inputs, values,
                                 static_cast<float*>(outbuf));
        // Ignore results that came from rows after the last LOD row
        for (std::size_t i = 0ul; i < (lodRegion.size % block16Height); ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            outputs[outputIndices[i]] = outbuf[i];
        }
    }
    // Process vertical remainder portion of matrix that's partitionable into 4x8 blocks
    for (; outputIndices < outputIndicesEnd; outputIndices += block8Height) {
        alignas(TF256::alignment()) float outbuf[block8Height];
        processBlocks8x8<TF256>(inputIndices, inputIndicesEndAlignedTo8, inputIndicesEnd, inputs, values,
                                static_cast<float*>(outbuf));
        // Ignore results that came from rows after the last LOD row
        auto maskOffStart = static_cast<std::size_t>(outputIndicesEnd - outputIndices);
        for (std::size_t i = 0; i < maskOffStart; ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            outputs[outputIndices[i]] = outbuf[i];
        }
    }
}

template<typename TF256, typename T>
struct Block8JointCalculationStrategy : public JointCalculationStrategy<T> {

    using JointGroupArrayView = typename JointCalculationStrategy<T>::JointGroupArrayView;

    void calculate(const JointGroupArrayView& jointGroups,
                   ConstArrayView<float> inputs,
                   ArrayView<float> outputs,
                   std::uint16_t lod) const override {

        for (std::size_t i = 0ul; i < jointGroups.size(); ++i) {
            processJointGroupBlock8<TF256>(jointGroups[i], inputs, outputs, lod);
        }
    }

    void calculate(const JointGroupArrayView& jointGroups,
                   ConstArrayView<float> inputs,
                   ArrayView<float> outputs,
                   std::uint16_t lod,
                   std::uint16_t jointGroupIndex) const override {

        processJointGroupBlock8<TF256>(jointGroups[jointGroupIndex], inputs, outputs, lod);
    }

};

}  // namespace bpcm

}  // namespace rl4
