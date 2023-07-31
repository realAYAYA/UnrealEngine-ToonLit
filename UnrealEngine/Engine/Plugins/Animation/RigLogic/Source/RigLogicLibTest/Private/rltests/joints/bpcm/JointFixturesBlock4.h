// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "rltests/dna/FakeReader.h"
#include "rltests/joints/bpcm/StorageValueType.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/Extent.h"
#include "riglogic/joints/bpcm/JointGroup.h"
#include "riglogic/joints/bpcm/JointsEvaluator.h"
#include "riglogic/joints/bpcm/LODRegion.h"
#include "riglogic/riglogic/RigLogic.h"

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

namespace block4 {

using namespace rl4;

namespace unoptimized {

extern const std::uint16_t lodCount;
extern const bpcm::Extent dimensions;
extern const Matrix<float> values;
extern const Matrix<std::uint16_t> inputIndices;
extern const Matrix<std::uint16_t> outputIndices;
extern const Matrix<std::uint16_t> lods;
extern const Vector<bpcm::Extent> subMatrices;
extern const Vector<float> neutralJointTranslationXs;
extern const Vector<float> neutralJointTranslationYs;
extern const Vector<float> neutralJointTranslationZs;
extern const Vector<float> neutralJointRotationXs;
extern const Vector<float> neutralJointRotationYs;
extern const Vector<float> neutralJointRotationZs;

}  // namespace unoptimized

namespace optimized {

extern const bpcm::Extent dimensions;
extern const AlignedMatrix<float> floatValues;
extern const AlignedMatrix<std::uint16_t> halfFloatValues;
extern const AlignedMatrix<std::uint16_t> inputIndices;
extern const AlignedMatrix<std::uint16_t> outputIndices;
extern const Vector<bpcm::JointGroup> jointGroups;
extern const Matrix<bpcm::LODRegion> lodRegions;

}  // namespace optimized

namespace input {

// Calculation input values
extern const Vector<float> values;

}  // namespace input

namespace output {

// Calculation output values
extern const Matrix<float> valuesPerLOD;

}  // namespace output

class CanonicalReader : public dna::FakeReader {
    public:
        ~CanonicalReader();

        std::uint16_t getLODCount() const override {
            return unoptimized::lodCount;
        }

        std::uint16_t getJointCount() const override {
            return static_cast<std::uint16_t>(unoptimized::neutralJointTranslationXs.size());
        }

        ConstArrayView<float> getNeutralJointTranslationXs() const override {
            return ConstArrayView<float>{unoptimized::neutralJointTranslationXs};
        }

        ConstArrayView<float> getNeutralJointTranslationYs() const override {
            return ConstArrayView<float>{unoptimized::neutralJointTranslationYs};
        }

        ConstArrayView<float> getNeutralJointTranslationZs() const override {
            return ConstArrayView<float>{unoptimized::neutralJointTranslationZs};
        }

        ConstArrayView<float> getNeutralJointRotationXs() const override {
            return ConstArrayView<float>{unoptimized::neutralJointRotationXs};
        }

        ConstArrayView<float> getNeutralJointRotationYs() const override {
            return ConstArrayView<float>{unoptimized::neutralJointRotationYs};
        }

        ConstArrayView<float> getNeutralJointRotationZs() const override {
            return ConstArrayView<float>{unoptimized::neutralJointRotationZs};
        }

        std::uint16_t getJointRowCount() const override {
            return unoptimized::dimensions.rows;
        }

        std::uint16_t getJointColumnCount() const override {
            return unoptimized::dimensions.cols;
        }

        std::uint16_t getJointGroupCount() const override {
            return static_cast<std::uint16_t>(unoptimized::subMatrices.size());
        }

        ConstArrayView<std::uint16_t> getJointGroupLODs(std::uint16_t jointGroupIndex) const override {
            return ConstArrayView<std::uint16_t>{unoptimized::lods[jointGroupIndex]};
        }

        ConstArrayView<std::uint16_t> getJointGroupInputIndices(std::uint16_t jointGroupIndex) const override {
            return ConstArrayView<std::uint16_t>{unoptimized::inputIndices[jointGroupIndex]};
        }

        ConstArrayView<std::uint16_t> getJointGroupOutputIndices(std::uint16_t jointGroupIndex) const override {
            return ConstArrayView<std::uint16_t>{unoptimized::outputIndices[jointGroupIndex]};
        }

        ConstArrayView<float> getJointGroupValues(std::uint16_t jointGroupIndex) const override {
            return ConstArrayView<float>{unoptimized::values[jointGroupIndex]};
        }

};

template<typename TValue>
struct OptimizedStorage {
    using StrategyPtr = std::unique_ptr<bpcm::JointCalculationStrategy<TValue>,
                                        std::function<void (bpcm::JointCalculationStrategy<TValue>*)> >;

    static bpcm::Evaluator<TValue> create(StrategyPtr strategy, rl4::MemoryResource* memRes);
    static bpcm::Evaluator<TValue> create(StrategyPtr strategy, std::uint16_t jointGroupIndex, rl4::MemoryResource* memRes);

};

}  // namespace block4
