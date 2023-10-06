// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/NeuralNet.h"

namespace rl4 {

namespace ml {

namespace cpu {

template<typename T>
class LayerEvaluator {
    public:
        using Pointer = typename UniqueInstance<LayerEvaluator>::PointerType;

    public:
        virtual ~LayerEvaluator() = default;

        virtual void calculate(const NeuralNetLayer<T>& layer, ConstArrayView<float> inputs, ArrayView<float> outputs) const = 0;
};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
