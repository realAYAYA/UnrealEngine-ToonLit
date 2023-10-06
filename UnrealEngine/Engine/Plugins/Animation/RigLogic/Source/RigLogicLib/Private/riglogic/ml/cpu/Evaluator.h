// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/ml/LODSpec.h"
#include "riglogic/ml/cpu/Inference.h"
#include "riglogic/ml/cpu/NeuralNet.h"
#include "riglogic/ml/cpu/OutputInstance.h"

#include <cstddef>

namespace rl4 {

namespace ml {

namespace cpu {

template<typename T, typename TF256, typename TF128>
class Evaluator : public MachineLearnedBehaviorEvaluator {
    public:
        using NeuralNetVectorType = Vector<NeuralNetInference<T, TF256, TF128> >;

        struct Accessor;
        friend Accessor;

    public:
        Evaluator(LODSpec&& lods_,
                  NeuralNetVectorType&& neuralNets_,
                  Vector<std::uint32_t>&& maxLayerOutputCounts_,
                  OutputInstance::Factory instanceFactory_) :
            lods{std::move(lods_)},
            neuralNets{std::move(neuralNets_)},
            maxLayerOutputCounts{std::move(maxLayerOutputCounts_)},
            instanceFactory{instanceFactory_} {
        }

        MachineLearnedBehaviorOutputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const override {
            return instanceFactory(ConstArrayView<std::uint32_t>{maxLayerOutputCounts}, instanceMemRes);
        }

        void calculate(ControlsInputInstance* inputs, MachineLearnedBehaviorOutputInstance* intermediateOutputs,
                       std::uint16_t lod) const override {
            assert(lod < lods.netIndicesPerLOD.size());
            const auto& netIndices = lods.netIndicesPerLOD[lod];
            const auto inputBuffer = inputs->getInputBuffer();
            const auto masks = intermediateOutputs->getMaskBuffer();
            for (const auto neuralNetIndex : netIndices) {
                assert(neuralNetIndex < masks.size());
                assert(neuralNetIndex < neuralNets.size());
                const float weight = masks[neuralNetIndex];
                auto layerBuffer1 = static_cast<OutputInstance*>(intermediateOutputs)->getOutputBuffer1(neuralNetIndex);
                auto layerBuffer2 = static_cast<OutputInstance*>(intermediateOutputs)->getOutputBuffer2(neuralNetIndex);
                // inputBuffer is also outputBuffer
                neuralNets[neuralNetIndex].calculate(inputBuffer, layerBuffer1, layerBuffer2, inputBuffer, weight);
            }
        }

        void calculate(ControlsInputInstance* inputs,
                       MachineLearnedBehaviorOutputInstance* intermediateOutputs,
                       std::uint16_t lod,
                       std::uint16_t neuralNetIndex) const override {
            assert(lod < lods.netIndicesPerLOD.size());
            static_cast<void>(lod);

            const auto masks = intermediateOutputs->getMaskBuffer();
            assert(neuralNetIndex < masks.size());
            assert(neuralNetIndex < neuralNets.size());
            const float weight = masks[neuralNetIndex];
            auto inputBuffer = inputs->getInputBuffer();
            auto layerBuffer1 = static_cast<OutputInstance*>(intermediateOutputs)->getOutputBuffer1(neuralNetIndex);
            auto layerBuffer2 = static_cast<OutputInstance*>(intermediateOutputs)->getOutputBuffer2(neuralNetIndex);
            // inputBuffer is also outputBuffer
            neuralNets[neuralNetIndex].calculate(inputBuffer, layerBuffer1, layerBuffer2, inputBuffer, weight);
        }

        void load(terse::BinaryInputArchive<BoundedIOStream>& archive) override {
            archive(lods, neuralNets, maxLayerOutputCounts);
        }

        void save(terse::BinaryOutputArchive<BoundedIOStream>& archive) override {
            archive(lods, neuralNets, maxLayerOutputCounts);
        }

    private:
        LODSpec lods;
        NeuralNetVectorType neuralNets;
        Vector<std::uint32_t> maxLayerOutputCounts;
        OutputInstance::Factory instanceFactory;
};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
