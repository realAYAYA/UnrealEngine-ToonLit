// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/types/Aliases.h"
#include "riglogic/ml/MachineLearnedBehaviorEvaluator.h"
#include "riglogic/ml/MachineLearnedBehaviorOutputInstance.h"

#include <cstddef>

namespace rl4 {

class ControlsInputInstance;

class MachineLearnedBehavior {
    public:
        using Pointer = UniqueInstance<MachineLearnedBehavior>::PointerType;

    public:
        MachineLearnedBehavior(MachineLearnedBehaviorEvaluator::Pointer evaluator_, MemoryResource* memRes);
        MachineLearnedBehavior(MachineLearnedBehaviorEvaluator::Pointer evaluator_,
                               Vector<Matrix<std::uint16_t> >&& neuralNetworkIndicesPerMeshRegion_);

        MachineLearnedBehaviorOutputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const;
        void calculate(ControlsInputInstance* inputs, MachineLearnedBehaviorOutputInstance* intermediateOutputs,
                       std::uint16_t lod) const;
        void calculate(ControlsInputInstance* inputs,
                       MachineLearnedBehaviorOutputInstance* intermediateOutputs,
                       std::uint16_t lod,
                       std::uint16_t neuralNetIndex) const;

        template<class Archive>
        void load(Archive& archive) {
            evaluator->load(archive);
            archive >> neuralNetworkIndicesPerMeshRegion;
        }

        template<class Archive>
        void save(Archive& archive) {
            evaluator->save(archive);
            archive << neuralNetworkIndicesPerMeshRegion;
        }

        std::uint16_t getMeshCount() const;
        std::uint16_t getMeshRegionCount(std::uint16_t meshIndex) const;
        ConstArrayView<std::uint16_t> getNeuralNetworkIndices(std::uint16_t meshIndex, std::uint16_t regionIndex) const;

    private:
        MachineLearnedBehaviorEvaluator::Pointer evaluator;
        Vector<Matrix<std::uint16_t> > neuralNetworkIndicesPerMeshRegion;

};

}  // namespace rl4
