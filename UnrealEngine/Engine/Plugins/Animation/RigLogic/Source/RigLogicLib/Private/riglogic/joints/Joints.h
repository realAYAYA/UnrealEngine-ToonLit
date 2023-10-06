// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/joints/JointsOutputInstance.h"
#include "riglogic/transformation/Transformation.h"

#include <cstdint>

namespace rl4 {

class ControlsInputInstance;
class JointsOutputInstance;

class Joints {
    public:
        using Pointer = UniqueInstance<Joints>::PointerType;

    public:
        Joints(JointsEvaluator::Pointer evaluator_, MemoryResource* memRes);
        Joints(JointsEvaluator::Pointer evaluator_,
               Vector<float>&& neutralValues_,
               Matrix<std::uint16_t>&& variableAttributeIndices_,
               std::uint16_t jointGroupCount_);

        JointsOutputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const;
        void calculate(const ControlsInputInstance* inputs, JointsOutputInstance* outputs, std::uint16_t lod) const;
        void calculate(const ControlsInputInstance* inputs,
                       JointsOutputInstance* outputs,
                       std::uint16_t lod,
                       std::uint16_t jointGroupIndex) const;

        template<class Archive>
        void load(Archive& archive) {
            evaluator->load(archive);
            archive >> neutralValues >> variableAttributeIndices >> jointGroupCount;
        }

        template<class Archive>
        void save(Archive& archive) {
            evaluator->save(archive);
            archive << neutralValues << variableAttributeIndices << jointGroupCount;
        }

        std::uint16_t getJointGroupCount() const;
        ConstArrayView<float> getRawNeutralValues() const;
        TransformationArrayView getNeutralValues() const;
        ConstArrayView<std::uint16_t> getVariableAttributeIndices(std::uint16_t lod) const;

    private:
        JointsEvaluator::Pointer evaluator;
        Vector<float> neutralValues;
        Matrix<std::uint16_t> variableAttributeIndices;
        std::uint16_t jointGroupCount;

};

}  // namespace rl4
