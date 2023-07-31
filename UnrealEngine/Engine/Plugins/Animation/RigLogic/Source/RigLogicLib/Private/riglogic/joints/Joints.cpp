// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/Joints.h"

#include "riglogic/riglogic/RigInstance.h"
#include "riglogic/transformation/Transformation.h"
#include "riglogic/types/Aliases.h"

#include <cstdint>

namespace rl4 {

Joints::Joints(JointsEvaluatorPtr evaluator_, MemoryResource* memRes) :
    evaluator{std::move(evaluator_)},
    neutralValues{memRes},
    variableAttributeIndices{memRes},
    jointGroupCount{} {
}

Joints::Joints(JointsEvaluatorPtr evaluator_,
               Vector<float>&& neutralValues_,
               Matrix<std::uint16_t>&& variableAttributeIndices_,
               std::uint16_t jointGroupCount_) :
    evaluator{std::move(evaluator_)},
    neutralValues{std::move(neutralValues_)},
    variableAttributeIndices{std::move(variableAttributeIndices_)},
    jointGroupCount{jointGroupCount_} {
}

void Joints::calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const {
    evaluator->calculate(inputs, outputs, lod);
}

void Joints::calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod,
                       std::uint16_t jointGroupIndex) const {
    evaluator->calculate(inputs, outputs, lod, jointGroupIndex);
}

ConstArrayView<float> Joints::getRawNeutralValues() const {
    return ConstArrayView<float>{neutralValues};
}

TransformationArrayView Joints::getNeutralValues() const {
    return TransformationArrayView{neutralValues.data(), neutralValues.size()};
}

std::uint16_t Joints::getJointGroupCount() const {
    return jointGroupCount;
}

ConstArrayView<std::uint16_t> Joints::getVariableAttributeIndices(std::uint16_t lod) const {
    return (lod < variableAttributeIndices.size()
            ? ConstArrayView<std::uint16_t>{variableAttributeIndices[lod]}
            : ConstArrayView<std::uint16_t>{});
}

}  // namespace rl4
