// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/LODSpec.h"
#include "riglogic/ml/MachineLearnedBehaviorEvaluator.h"
#include "riglogic/ml/cpu/Evaluator.h"
#include "riglogic/ml/cpu/Inference.h"
#include "riglogic/ml/cpu/NeuralNet.h"
#include "riglogic/ml/cpu/OutputInstance.h"
#include "riglogic/types/bpcm/Optimizer.h"
#include "riglogic/utils/Extd.h"

namespace rl4 {

namespace ml {

namespace cpu {

template<typename T, typename TF256, typename TF128>
class Factory {
    public:
        static MachineLearnedBehaviorEvaluator::Pointer create(const dna::MachineLearnedBehaviorReader* reader,
                                                               MemoryResource* memRes) {
            Vector<NeuralNetInference<T, TF256, TF128> > neuralNets{memRes};
            Vector<std::uint32_t> maxLayerOutputCountPerNet{memRes};
            auto instanceFactory = [](ConstArrayView<std::uint32_t> maxLayerOutputCounts, MemoryResource* instanceMemRes) {
                    using OutputInstancePointer = UniqueInstance<OutputInstance, MachineLearnedBehaviorOutputInstance>;
                    return OutputInstancePointer::with(instanceMemRes).create(maxLayerOutputCounts, instanceMemRes);
                };
            auto factory = UniqueInstance<Evaluator<T, TF256, TF128>, MachineLearnedBehaviorEvaluator>::with(memRes);

            if (reader == nullptr) {
                return factory.create(LODSpec{memRes},
                                      std::move(neuralNets),
                                      std::move(maxLayerOutputCountPerNet),
                                      instanceFactory);
            }

            auto lods = computeLODs(reader, memRes);
            maxLayerOutputCountPerNet.resize(lods.netCount);
            neuralNets.reserve(lods.netCount);

            for (std::uint16_t neuralNetIdx = {}; neuralNetIdx < lods.netCount; ++neuralNetIdx) {
                const auto layerCount = reader->getNeuralNetworkLayerCount(neuralNetIdx);
                if (layerCount != 0u) {
                    auto net = createNeuralNet(reader,
                                               neuralNetIdx,
                                               layerCount,
                                               &maxLayerOutputCountPerNet[neuralNetIdx],
                                               memRes);
                    neuralNets.emplace_back(std::move(net), memRes);
                }
            }

            return factory.create(std::move(lods),
                                  std::move(neuralNets),
                                  std::move(maxLayerOutputCountPerNet),
                                  instanceFactory);
        }

    private:
        static NeuralNet<T> createNeuralNet(const dna::MachineLearnedBehaviorReader* reader,
                                            std::uint16_t neuralNetIdx,
                                            std::uint16_t layerCount,
                                            std::uint32_t* maxLayerOutputCount,
                                            MemoryResource* memRes) {
            const auto inputIndices = reader->getNeuralNetworkInputIndices(neuralNetIdx);
            const auto outputIndices = reader->getNeuralNetworkOutputIndices(neuralNetIdx);
            std::uint32_t inputCount = static_cast<std::uint32_t>(inputIndices.size());

            NeuralNet<T> neuralNet{memRes};
            * maxLayerOutputCount = inputCount;
            neuralNet.layers.reserve(layerCount);
            for (std::uint16_t layerIdx = {}; layerIdx < layerCount; ++layerIdx) {
                const auto weights = reader->getNeuralNetworkLayerWeights(neuralNetIdx, layerIdx);
                const auto biases = reader->getNeuralNetworkLayerBiases(neuralNetIdx, layerIdx);
                const auto activationFunction = reader->getNeuralNetworkLayerActivationFunction(neuralNetIdx, layerIdx);
                const auto activationFunctionParams = reader->getNeuralNetworkLayerActivationFunctionParameters(neuralNetIdx,
                                                                                                                layerIdx);
                const auto outputCount = static_cast<std::uint32_t>(biases.size());
                const auto layer = createLayer(inputCount,
                                               outputCount,
                                               weights,
                                               biases,
                                               activationFunction,
                                               activationFunctionParams,
                                               memRes);
                // Keep track of the layer with the largest number of outputs
                // In the next layer, the current output count becomes the input count
                * maxLayerOutputCount = std::max(*maxLayerOutputCount, layer.weights.padded.rows);
                neuralNet.layers.push_back(std::move(layer));
                inputCount = outputCount;
            }

            neuralNet.inputIndices.assign(inputIndices.begin(), inputIndices.end());
            neuralNet.outputIndices.assign(outputIndices.begin(), outputIndices.end());
            return neuralNet;
        }

        static NeuralNetLayer<T> createLayer(std::uint32_t inputCount,
                                             std::uint32_t outputCount,
                                             ConstArrayView<float> weights,
                                             ConstArrayView<float> biases,
                                             dna::ActivationFunction activationFunction,
                                             ConstArrayView<float> activationFunctionParams,
                                             MemoryResource* memRes) {
            NeuralNetLayer<T> layer{memRes};

            layer.weights.original = {outputCount, inputCount};
            const std::uint32_t padding = extd::roundUp(layer.weights.original.rows, block4Height) - layer.weights.original.rows;
            layer.weights.padded = {layer.weights.original.rows + padding, layer.weights.original.cols};
            layer.weights.rows =
                AlignedBlockView{layer.weights.original.rows, layer.weights.padded.rows, block8Height, block4Height};
            layer.weights.cols = AlignedBlockView{
                layer.weights.padded.cols,
                layer.weights.padded.cols - (layer.weights.padded.cols % 4u),
                layer.weights.padded.cols - (layer.weights.padded.cols % 8u)
            };
            layer.weights.values.resize(layer.weights.padded.size());
            bpcm::Optimizer<TF128, block8Height, block4Height>::optimize(layer.weights.values.data(),
                                                                         weights.data(),
                                                                         layer.weights.original);

            layer.biases.resize(layer.weights.padded.rows);
            bpcm::Optimizer<TF128, block8Height, block4Height>::optimize(layer.biases.data(), biases.data(),
                                                                         Extent{static_cast<std::uint32_t>(biases.size()), 1u});

            layer.activationFunction = activationFunction;
            layer.activationFunctionParameters.assign(activationFunctionParams.begin(), activationFunctionParams.end());
            return layer;
        }

        static LODSpec computeLODs(const dna::MachineLearnedBehaviorReader* reader, MemoryResource* memRes) {
            LODSpec lods{memRes};
            const auto lodCount = reader->getLODCount();
            lods.netIndicesPerLOD.resize(lodCount);
            for (std::uint16_t lod = {}; lod < lodCount; ++lod) {
                const auto netIndices = reader->getNeuralNetworkIndicesForLOD(lod);
                lods.netIndicesPerLOD[lod].assign(netIndices.begin(), netIndices.end());
            }
            lods.netCount = reader->getNeuralNetworkCount();
            return lods;
        }

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
