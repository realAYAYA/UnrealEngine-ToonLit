// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/Consts.h"
#include "riglogic/utils/Macros.h"

namespace rl4 {

namespace ml {

namespace cpu {

template<typename TF256, typename T>
static FORCE_INLINE void processBlocks8x1(const float* inputVectorEndAlignedTo4,
                                          const float* inputVectorEnd,
                                          const T*& weights,
                                          TF256& sum) {
    TF256 remainder{};
    for (const float* inputVector = inputVectorEndAlignedTo4;
         inputVector < inputVectorEnd;
         ++inputVector, weights += block8Height) {
        const TF256 input{*inputVector};
        const TF256 blk = TF256::fromAlignedSource(weights);
        remainder += (blk * input);
    }
    sum += remainder;
}

template<typename TF256, typename TActivationFunction, typename T>
static FORCE_INLINE void processBlocks8x4(const float* inputVectorStart,
                                          const float* inputVectorEndAlignedTo4,
                                          const float* inputVectorEnd,
                                          const T*& weights,
                                          const T* biases,
                                          const float* activationParams,
                                          float* outbuf) {
    TF256 sum1{};
    TF256 sum2{};
    TF256 sum3{};
    TF256 sum4{};
    for (const float* inputVector = inputVectorStart;
         inputVector < inputVectorEndAlignedTo4;
         inputVector += block8Width, weights += block8Height * block8Width) {
        const TF256 input1{inputVector[0]};
        const TF256 input2{inputVector[1]};
        const TF256 input3{inputVector[2]};
        const TF256 input4{inputVector[3]};
        const TF256 blk1 = TF256::fromAlignedSource(weights + TF256::size() * 0);
        const TF256 blk2 = TF256::fromAlignedSource(weights + TF256::size() * 1);
        const TF256 blk3 = TF256::fromAlignedSource(weights + TF256::size() * 2);
        const TF256 blk4 = TF256::fromAlignedSource(weights + TF256::size() * 3);
        /*
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
        */
        sum1 += (blk1 * input1);
        sum2 += (blk2 * input2);
        sum3 += (blk3 * input3);
        sum4 += (blk4 * input4);
    }
    // Process 8x1 horizontal remainder portion after 8x4 blocks are consumed
    processBlocks8x1(inputVectorEndAlignedTo4, inputVectorEnd, weights, sum1);

    const TF256 bias = TF256::fromAlignedSource(biases);

    sum1 += sum3;
    sum2 += sum4;
    sum1 += sum2;

    sum1 += bias;

    TActivationFunction{} (sum1, activationParams);

    sum1.alignedStore(outbuf);
}

template<typename TF128, typename T>
static FORCE_INLINE void processBlocks4x1(const float* inputVectorEndAlignedTo8,
                                          const float* inputVectorEnd,
                                          const T*& weights,
                                          TF128& sum1) {
    TF128 remainder{};
    for (const float* inputVector = inputVectorEndAlignedTo8;
         inputVector < inputVectorEnd;
         ++inputVector, weights += block4Height) {
        const TF128 input{*inputVector};
        const TF128 blk = TF128::fromAlignedSource(weights);
        remainder += (blk * input);
    }
    sum1 += remainder;
}

template<typename TF128, typename TActivationFunction, typename T>
static FORCE_INLINE void processBlocks4x8(const float* inputVectorStart,
                                          const float* inputVectorEndAlignedTo8,
                                          const float* inputVectorEnd,
                                          const T*& weights,
                                          const T* biases,
                                          const float* activationParams,
                                          float* outbuf) {
    TF128 sum1{};
    TF128 sum2{};
    TF128 sum3{};
    TF128 sum4{};
    TF128 sum5{};
    TF128 sum6{};
    TF128 sum7{};
    TF128 sum8{};
    for (const float* inputVector = inputVectorStart;
         inputVector < inputVectorEndAlignedTo8;
         inputVector += block4Width, weights += block4Height * block4Width) {
        const TF128 input1{inputVector[0]};
        const TF128 input2{inputVector[1]};
        const TF128 input3{inputVector[2]};
        const TF128 input4{inputVector[3]};
        const TF128 input5{inputVector[4]};
        const TF128 input6{inputVector[5]};
        const TF128 input7{inputVector[6]};
        const TF128 input8{inputVector[7]};
        const TF128 blk1 = TF128::fromAlignedSource(weights);
        const TF128 blk2 = TF128::fromAlignedSource(weights + TF128::size());
        const TF128 blk3 = TF128::fromAlignedSource(weights + TF128::size() * 2);
        const TF128 blk4 = TF128::fromAlignedSource(weights + TF128::size() * 3);
        const TF128 blk5 = TF128::fromAlignedSource(weights + TF128::size() * 4);
        const TF128 blk6 = TF128::fromAlignedSource(weights + TF128::size() * 5);
        const TF128 blk7 = TF128::fromAlignedSource(weights + TF128::size() * 6);
        const TF128 blk8 = TF128::fromAlignedSource(weights + TF128::size() * 7);
        sum1 += (blk1 * input1);
        sum2 += (blk2 * input2);
        sum3 += (blk3 * input3);
        sum4 += (blk4 * input4);
        sum5 += (blk5 * input5);
        sum6 += (blk6 * input6);
        sum7 += (blk7 * input7);
        sum8 += (blk8 * input8);
    }
    // Process 4x1 horizontal remainder portion after 4x8 blocks are consumed
    processBlocks4x1(inputVectorEndAlignedTo8, inputVectorEnd, weights, sum1);

    const TF128 bias1 = TF128::fromAlignedSource(biases);

    sum1 += sum2;
    sum3 += sum4;
    sum5 += sum6;
    sum7 += sum8;
    sum1 += sum3;
    sum5 += sum7;
    sum1 += sum5;

    sum1 += bias1;

    TActivationFunction{} (sum1, activationParams);

    sum1.alignedStore(outbuf);
}

template<typename TF256, typename TF128, template<class ...> class TActivationFunction, typename T>
static FORCE_INLINE void calculateBlock4(const NeuralNetLayer<T>& layer, ConstArrayView<float> inputs, ArrayView<float> outputs) {
    float* outputVector = outputs.data();
    const float* outputVectorEndAlignedToLastBlock8 = outputVector + layer.weights.rows.sizeAlignedToLastFullBlock;
    float* outputVectorEnd = outputVector + layer.weights.rows.size;

    const float* inputVector = inputs.data();
    const float* inputVectorEndAlignedTo4 = inputVector + layer.weights.cols.sizeAlignedToLastFullBlock;
    const float* inputVectorEndAlignedTo8 = inputVector + layer.weights.cols.sizeAlignedToSecondLastFullBlock;
    const float* inputVectorEnd = inputVector + layer.weights.cols.size;

    const T* weights = layer.weights.values.data();
    const T* biases = layer.biases.data();
    const float* activationParams = layer.activationFunctionParameters.data();

    for (; outputVector < outputVectorEndAlignedToLastBlock8; outputVector += block8Height, biases += block8Height) {
        alignas(TF256::alignment()) float outbuf[block8Height];
        processBlocks8x4<TF256, TActivationFunction<TF256> >(inputVector,
                                                             inputVectorEndAlignedTo4,
                                                             inputVectorEnd,
                                                             weights,
                                                             biases,
                                                             activationParams,
                                                             outbuf);
        std::memcpy(outputVector, outbuf, sizeof(outbuf));
    }

    for (; outputVector < outputVectorEnd; outputVector += block4Height, biases += block4Height) {
        alignas(TF128::alignment()) float outbuf[block4Height];
        processBlocks4x8<TF128, TActivationFunction<TF128> >(inputVector,
                                                             inputVectorEndAlignedTo8,
                                                             inputVectorEnd,
                                                             weights,
                                                             biases,
                                                             activationParams,
                                                             outbuf);
        std::memcpy(outputVector, outbuf, sizeof(outbuf));
    }
}

}  // namespace rl4

}  // namespace ml

}  // namespace cpu

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
