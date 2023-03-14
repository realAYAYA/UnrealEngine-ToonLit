// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/transformation/Transformation.h"
#include "riglogic/types/Aliases.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

class RigInstance;

class Joints {
    public:
        Joints(JointsEvaluatorPtr evaluator_, MemoryResource* memRes);
        Joints(JointsEvaluatorPtr evaluator_,
               Vector<float>&& neutralValues_,
               Matrix<std::uint16_t>&& variableAttributeIndices_,
               std::uint16_t jointGroupCount_);

        void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const;
        void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod,
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
        JointsEvaluatorPtr evaluator;
        Vector<float> neutralValues;
        Matrix<std::uint16_t> variableAttributeIndices;
        std::uint16_t jointGroupCount;

};

}  // namespace rl4
