// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/Block4.h"
#include "riglogic/ml/cpu/Consts.h"
#include "riglogic/ml/cpu/layers/LayerEvaluator.h"
#include "riglogic/system/simd/SIMD.h"
#include "riglogic/utils/Macros.h"

namespace rl4 {

namespace ml {

namespace cpu {

template<typename TFVec>
struct LeakyReLUActivationFunction {

    void operator()(TFVec& sum, const float* activationParams) {
        const TFVec alpha{* activationParams};
        const TFVec zero{};
        const TFVec mask = (sum < zero);
        const TFVec multiplied = (sum * alpha);
        sum = trimd::andnot(mask, sum) | (multiplied & mask);
    }

};

template<typename T, typename TF256, typename TF128>
class LeakyReLULayerEvaluator : public LayerEvaluator<T> {
    public:
        void calculate(const NeuralNetLayer<T>& layer, ConstArrayView<float> inputs, ArrayView<float> outputs) const override {
            calculateBlock4<TF256, TF128, LeakyReLUActivationFunction>(layer, inputs, outputs);
        }

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
