// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/bpcm/CalculationStrategy.h"
#include "riglogic/joints/bpcm/Evaluator.h"
#include "riglogic/joints/bpcm/BPCMOutputInstance.h"
#include "riglogic/joints/bpcm/Storage.h"
#include "riglogic/joints/bpcm/StorageSize.h"
#include "riglogic/riglogic/RigMetrics.h"
#include "riglogic/types/Aliases.h"
#include "riglogic/types/bpcm/Optimizer.h"
#include "riglogic/utils/Extd.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace bpcm {

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
class JointsBuilderCommon : public JointsBuilder {
    protected:
        using CalculationStrategy = JointCalculationStrategy<TValue>;
        using CalculationStrategyPtr = std::unique_ptr<CalculationStrategy, std::function<void (CalculationStrategy*)> >;

    protected:
        JointsBuilderCommon(CalculationStrategyPtr strategy_, MemoryResource* memRes_);

    public:
        void computeStorageRequirements(const RigMetrics& source) override;
        void computeStorageRequirements(const dna::BehaviorReader* source) override;
        void allocateStorage(const dna::BehaviorReader* source) override;
        void fillStorage(const dna::BehaviorReader* source) override;
        JointsEvaluator::Pointer build() override;

    protected:
        void setValues(const dna::BehaviorReader* source);
        void setInputIndices(const dna::BehaviorReader* source);
        void setOutputIndices(const dna::BehaviorReader* source);
        void setLODs(const dna::BehaviorReader* source);

    protected:
        MemoryResource* memRes;
        StorageSize sizeReqs;

        JointStorage<TValue> storage;
        CalculationStrategyPtr strategy;
};

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::JointsBuilderCommon(CalculationStrategyPtr strategy_,
                                                                            MemoryResource* memRes_) :
    memRes{memRes_},
    sizeReqs{memRes},
    storage{memRes},
    strategy{std::move(strategy_)} {
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::computeStorageRequirements(const RigMetrics& source) {
    // This is incomplete, but enough to create valid joint output instances when restoring from a dump.
    // Joint attribute count is the only needed data by the instance factory.
    sizeReqs.attributeCount = source.jointAttributeCount;
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::computeStorageRequirements(const dna::BehaviorReader* source) {
    sizeReqs.computeFrom(source, PadTo);
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::allocateStorage(const dna::BehaviorReader*  /*unused*/) {
    storage.values.resize(sizeReqs.valueCount);
    storage.inputIndices.resize(sizeReqs.inputIndexCount);
    storage.outputIndices.resize(sizeReqs.outputIndexCount);
    storage.lodRegions.reserve(sizeReqs.lodRegionCount);
    storage.jointGroups.resize(sizeReqs.jointGroups.size());
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::fillStorage(const dna::BehaviorReader* source) {
    setValues(source);
    setInputIndices(source);
    setOutputIndices(source);
    setLODs(source);
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::setValues(const dna::BehaviorReader* source) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < source->getJointGroupCount(); ++i) {
        const auto values = source->getJointGroupValues(i);
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        storage.jointGroups[i].valuesOffset = offset;
        storage.jointGroups[i].valuesSize = jointGroupSize.padded.size();
        offset += Optimizer<TFVec, BlockHeight, PadTo>::optimize(storage.values.data() + offset,
                                                                 values.data(),
                                                                 jointGroupSize.original);
    }
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::setInputIndices(const dna::BehaviorReader* source) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < source->getJointGroupCount(); ++i) {
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        const auto indices = source->getJointGroupInputIndices(i);
        std::copy(indices.begin(), indices.end(), extd::advanced(storage.inputIndices.begin(), offset));
        storage.jointGroups[i].inputIndicesOffset = offset;
        storage.jointGroups[i].inputIndicesSize = jointGroupSize.padded.cols;
        storage.jointGroups[i].inputIndicesSizeAlignedTo4 = jointGroupSize.padded.cols - (jointGroupSize.padded.cols % 4u);
        storage.jointGroups[i].inputIndicesSizeAlignedTo8 = jointGroupSize.padded.cols - (jointGroupSize.padded.cols % 8u);
        offset += jointGroupSize.padded.cols;
    }
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::setOutputIndices(const dna::BehaviorReader* source) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < source->getJointGroupCount(); ++i) {
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        const auto indices = source->getJointGroupOutputIndices(i);
        std::copy(indices.begin(), indices.end(), extd::advanced(storage.outputIndices.begin(), offset));
        storage.jointGroups[i].outputIndicesOffset = offset;
        offset += jointGroupSize.padded.rows;
    }
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
void JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::setLODs(const dna::BehaviorReader* source) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < source->getJointGroupCount(); ++i) {
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        const auto dstRowCount = jointGroupSize.padded.rows;
        auto makeLODRegion = [dstRowCount](std::uint16_t lodRowCount) {
                return LODRegion{lodRowCount, dstRowCount, BlockHeight, PadTo};
            };
        const auto lods = source->getJointGroupLODs(i);
        std::transform(lods.begin(), lods.end(), std::back_inserter(storage.lodRegions), makeLODRegion);
        storage.jointGroups[i].lodsOffset = offset;
        offset += source->getLODCount();
    }
}

template<typename TValue, std::uint32_t BlockHeight, std::uint32_t PadTo, typename TFVec>
JointsEvaluator::Pointer JointsBuilderCommon<TValue, BlockHeight, PadTo, TFVec>::build() {
    const auto attributeCount = static_cast<std::uint16_t>(sizeReqs.attributeCount);
    auto instanceFactory = [attributeCount](MemoryResource* instanceMemRes) {
            return UniqueInstance<OutputInstance, JointsOutputInstance>::with(instanceMemRes).create(attributeCount,
                                                                                                     instanceMemRes);
        };
    auto factory = UniqueInstance<Evaluator<TValue>, JointsEvaluator>::with(memRes);
    return factory.create(std::move(storage), std::move(strategy), instanceFactory, memRes);
}

}  // namespace bpcm

}  // namespace rl4
