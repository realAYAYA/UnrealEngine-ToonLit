// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/ml/MachineLearnedBehavior.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/ml/MachineLearnedBehaviorOutputInstance.h"

#include <cassert>
#include <cstddef>

namespace rl4 {

MachineLearnedBehavior::MachineLearnedBehavior(MachineLearnedBehaviorEvaluator::Pointer evaluator_, MemoryResource* memRes) :
    evaluator{std::move(evaluator_)},
    neuralNetworkIndicesPerMeshRegion{memRes} {
}

MachineLearnedBehavior::MachineLearnedBehavior(MachineLearnedBehaviorEvaluator::Pointer evaluator_,
                                               Vector<Matrix<std::uint16_t> >&& neuralNetworkIndicesPerMeshRegion_) :
    evaluator{std::move(evaluator_)},
    neuralNetworkIndicesPerMeshRegion{std::move(neuralNetworkIndicesPerMeshRegion_)} {
}

MachineLearnedBehaviorOutputInstance::Pointer MachineLearnedBehavior::createInstance(MemoryResource* instanceMemRes) const {
    return evaluator->createInstance(instanceMemRes);
}

void MachineLearnedBehavior::calculate(ControlsInputInstance* inputs,
                                       MachineLearnedBehaviorOutputInstance* intermediateOutputs,
                                       std::uint16_t lod) const {
    evaluator->calculate(inputs, intermediateOutputs, lod);
}

void MachineLearnedBehavior::calculate(ControlsInputInstance* inputs,
                                       MachineLearnedBehaviorOutputInstance* intermediateOutputs,
                                       std::uint16_t lod,
                                       std::uint16_t neuralNetIndex) const {
    evaluator->calculate(inputs, intermediateOutputs, lod, neuralNetIndex);
}

std::uint16_t MachineLearnedBehavior::getMeshCount() const {
    return static_cast<std::uint16_t>(neuralNetworkIndicesPerMeshRegion.size());
}

std::uint16_t MachineLearnedBehavior::getMeshRegionCount(std::uint16_t meshIndex) const {
    assert(meshIndex < neuralNetworkIndicesPerMeshRegion.size());
    return static_cast<std::uint16_t>(neuralNetworkIndicesPerMeshRegion[meshIndex].size());
}

ConstArrayView<std::uint16_t> MachineLearnedBehavior::getNeuralNetworkIndices(std::uint16_t meshIndex,
                                                                              std::uint16_t regionIndex) const {
    assert(meshIndex < getMeshCount());
    assert(regionIndex < getMeshRegionCount(meshIndex));
    return ConstArrayView<std::uint16_t>{neuralNetworkIndicesPerMeshRegion[meshIndex][regionIndex]};
}

}  // namespace rl4
