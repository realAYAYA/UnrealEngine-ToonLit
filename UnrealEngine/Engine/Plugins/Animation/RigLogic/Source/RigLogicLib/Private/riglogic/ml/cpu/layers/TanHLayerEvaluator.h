// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/Block4.h"
#include "riglogic/ml/cpu/Consts.h"
#include "riglogic/ml/cpu/layers/LayerEvaluator.h"
#include "riglogic/ml/cpu/layers/Utils.h"
#include "riglogic/utils/Macros.h"

#include <cmath>
#include <type_traits>

namespace rl4 {

namespace ml {

namespace cpu {

template<typename TFVec, typename Enable = void>
struct TanHActivationFunction;


template<typename TF256>
struct TanHActivationFunction<TF256, typename std::enable_if<HasSize<TF256, 8ul>::value>::type> {

    void operator()(TF256& sum, const float*  /*unused*/) {
        // FIXME: dangerously inefficient code
        alignas(TF256::alignment()) float buf[TF256::size()];

        sum.alignedStore(buf);

        buf[0] = std::tanh(buf[0]);
        buf[1] = std::tanh(buf[1]);
        buf[2] = std::tanh(buf[2]);
        buf[3] = std::tanh(buf[3]);
        buf[4] = std::tanh(buf[4]);
        buf[5] = std::tanh(buf[5]);
        buf[6] = std::tanh(buf[6]);
        buf[7] = std::tanh(buf[7]);

        sum.alignedLoad(buf);
    }

};

template<typename TF128>
struct TanHActivationFunction<TF128, typename std::enable_if<HasSize<TF128, 4ul>::value>::type> {

    void operator()(TF128& sum, const float*  /*unused*/) {
        // FIXME: dangerously inefficient code
        alignas(TF128::alignment()) float buf[TF128::size()];

        sum.alignedStore(buf);

        buf[0] = std::tanh(buf[0]);
        buf[1] = std::tanh(buf[1]);
        buf[2] = std::tanh(buf[2]);
        buf[3] = std::tanh(buf[3]);

        sum.alignedLoad(buf);
    }

};

template<typename T, typename TF256, typename TF128>
class TanHLayerEvaluator : public LayerEvaluator<T> {
    public:
        void calculate(const NeuralNetLayer<T>& layer, ConstArrayView<float> inputs, ArrayView<float> outputs) const override {
            calculateBlock4<TF256, TF128, TanHActivationFunction>(layer, inputs, outputs);
        }

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
