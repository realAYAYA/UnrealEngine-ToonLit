// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/types/AlignedBlockView.h"
#include "riglogic/types/Extent.h"

#include <cstdint>

namespace rl4 {

namespace ml {

namespace cpu {

template<typename T>
struct WeightMatrix {
    Extent original;
    Extent padded;
    AlignedBlockView rows;
    AlignedBlockView cols;
    AlignedVector<T> values;

    explicit WeightMatrix(MemoryResource* memRes) :
        original{},
        padded{},
        rows{},
        cols{},
        values{memRes} {
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(original, padded, rows, cols, values);
    }

};

template<typename T>
struct NeuralNetLayer {
    WeightMatrix<T> weights;
    AlignedVector<T> biases;
    Vector<float> activationFunctionParameters;
    dna::ActivationFunction activationFunction;

    explicit NeuralNetLayer(MemoryResource* memRes) :
        weights{memRes},
        biases{memRes},
        activationFunctionParameters{memRes},
        activationFunction{} {
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(weights, biases, activationFunctionParameters, activationFunction);
    }

};

template<typename T>
struct NeuralNet {
    Vector<NeuralNetLayer<T> > layers;
    Vector<std::uint16_t> inputIndices;
    Vector<std::uint16_t> outputIndices;

    explicit NeuralNet(MemoryResource* memRes) : layers{memRes}, inputIndices{memRes}, outputIndices{memRes} {
    }

    NeuralNet(Vector<NeuralNetLayer<T> >&& layers_, Vector<std::uint16_t>&& inputIndices_,
              Vector<std::uint16_t>&& outputIndices_) :
        layers{std::move(layers_)},
        inputIndices{std::move(inputIndices_)},
        outputIndices{std::move(outputIndices_)} {
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(layers, inputIndices, outputIndices);
    }

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
