// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/NeuralNet.h"
#include "riglogic/ml/cpu/layers/LeakyReLULayerEvaluator.h"
#include "riglogic/ml/cpu/layers/LinearLayerEvaluator.h"
#include "riglogic/ml/cpu/layers/ReLULayerEvaluator.h"
#include "riglogic/ml/cpu/layers/SigmoidLayerEvaluator.h"
#include "riglogic/ml/cpu/layers/TanHLayerEvaluator.h"

namespace rl4 {

namespace ml {

namespace cpu {

template<typename T, typename TF256, typename TF128>
struct LayerEvaluatorFactory {

    static typename LayerEvaluator<T>::Pointer create(dna::ActivationFunction activationFunction, MemoryResource* memRes) {
        switch (activationFunction) {
            case dna::ActivationFunction::linear:
                return UniqueInstance<LinearLayerEvaluator<T, TF256, TF128>, LayerEvaluator<T> >::with(memRes).create();
            case dna::ActivationFunction::relu:
                return UniqueInstance<ReLULayerEvaluator<T, TF256, TF128>, LayerEvaluator<T> >::with(memRes).create();
            case dna::ActivationFunction::leakyrelu:
                return UniqueInstance<LeakyReLULayerEvaluator<T, TF256, TF128>, LayerEvaluator<T> >::with(memRes).create();
            case dna::ActivationFunction::tanh:
                return UniqueInstance<TanHLayerEvaluator<T, TF256, TF128>, LayerEvaluator<T> >::with(memRes).create();
            case dna::ActivationFunction::sigmoid:
                return UniqueInstance<SigmoidLayerEvaluator<T, TF256, TF128>, LayerEvaluator<T> >::with(memRes).create();
        }
        return nullptr;
    }

};

template<typename T, typename TF256, typename TF128>
struct NeuralNetInference {
    NeuralNet<T> neuralNet;
    Vector<typename LayerEvaluator<T>::Pointer> layerEvaluators;

    explicit NeuralNetInference(MemoryResource* memRes) : neuralNet{memRes}, layerEvaluators{memRes} {
    }

    NeuralNetInference(NeuralNet<T>&& neuralNet_, MemoryResource* memRes) : neuralNet{std::move(neuralNet_)},
        layerEvaluators{memRes} {
        createLayerEvaluators();
    }

    void calculate(ConstArrayView<float> inputBuffer, ArrayView<float> layerBuffer1, ArrayView<float> layerBuffer2,
                   ArrayView<float> outputBuffer, float weight) const {

        assert(neuralNet.inputIndices.size() <= layerBuffer1.size());
        assert(neuralNet.outputIndices.size() <= layerBuffer1.size());
        assert(neuralNet.inputIndices.size() <= layerBuffer2.size());
        assert(neuralNet.outputIndices.size() <= layerBuffer2.size());
        assert(layerEvaluators.size() == neuralNet.layers.size());

        if (weight == 0.0f) {
            for (std::size_t i = 0ul; i < neuralNet.outputIndices.size(); ++i) {
                outputBuffer[neuralNet.outputIndices[i]] = 0.0f;
            }
            return;
        }

        for (std::size_t i = 0ul; i < neuralNet.inputIndices.size(); ++i) {
            layerBuffer1[i] = inputBuffer[neuralNet.inputIndices[i]];
        }

        for (std::size_t layerIndex = 0u; layerIndex < neuralNet.layers.size(); ++layerIndex) {
            layerEvaluators[layerIndex]->calculate(neuralNet.layers[layerIndex], layerBuffer1, layerBuffer2);
            std::swap(layerBuffer1, layerBuffer2);
        }

        for (std::size_t i = 0ul; i < neuralNet.outputIndices.size(); ++i) {
            outputBuffer[neuralNet.outputIndices[i]] = layerBuffer1[i] * weight;
        }
    }

    template<class Archive>
    void load(Archive& archive) {
        archive(neuralNet);
        createLayerEvaluators();
    }

    template<class Archive>
    void save(Archive& archive) {
        archive(neuralNet);
    }

    void createLayerEvaluators() {
        layerEvaluators.resize(neuralNet.layers.size());
        auto memRes = layerEvaluators.get_allocator().getMemoryResource();
        for (std::size_t layerIndex = 0ul; layerIndex < neuralNet.layers.size(); ++layerIndex) {
            layerEvaluators[layerIndex] = LayerEvaluatorFactory<T, TF256, TF128>::create(
                neuralNet.layers[layerIndex].activationFunction,
                memRes);
        }
    }

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4
