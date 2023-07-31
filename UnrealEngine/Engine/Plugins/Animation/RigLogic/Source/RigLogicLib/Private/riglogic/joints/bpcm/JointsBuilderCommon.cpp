// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/bpcm/JointsBuilderCommon.h"

#include "riglogic/joints/bpcm/JointsEvaluator.h"
#include "riglogic/joints/bpcm/StorageSize.h"
#include "riglogic/utils/Extd.h"

#include <dna/layers/BehaviorReader.h>
#include <pma/utils/ManagedInstance.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace bpcm {

template<typename TValue>
JointsBuilderCommon<TValue>::JointsBuilderCommon(std::uint32_t blockHeight_, std::uint32_t padTo_, MemoryResource* memRes_) :
    blockHeight{blockHeight_},
    padTo{padTo_},
    memRes{memRes_},
    sizeReqs{memRes},
    storage{memRes} {
}

template<typename TValue>
void JointsBuilderCommon<TValue>::allocateStorage(const dna::BehaviorReader* reader) {
    sizeReqs.computeFrom(reader, padTo);
    storage.values.resize(sizeReqs.valueCount);
    storage.inputIndices.resize(sizeReqs.inputIndexCount);
    storage.outputIndices.resize(sizeReqs.outputIndexCount);
    storage.lodRegions.reserve(sizeReqs.lodRegionCount);
    storage.jointGroups.resize(sizeReqs.jointGroups.size());
}

template<typename TValue>
void JointsBuilderCommon<TValue>::fillStorage(const dna::BehaviorReader* reader) {
    setValues(reader);
    setInputIndices(reader);
    setOutputIndices(reader);
    setLODs(reader);
}

template<typename TValue>
void JointsBuilderCommon<TValue>::setInputIndices(const dna::BehaviorReader* reader) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < reader->getJointGroupCount(); ++i) {
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        const auto indices = reader->getJointGroupInputIndices(i);
        std::copy(indices.begin(), indices.end(), extd::advanced(storage.inputIndices.begin(), offset));
        storage.jointGroups[i].inputIndicesOffset = offset;
        storage.jointGroups[i].inputIndicesSize = jointGroupSize.padded.cols;
        storage.jointGroups[i].inputIndicesSizeAlignedTo4 = jointGroupSize.padded.cols - (jointGroupSize.padded.cols % 4u);
        storage.jointGroups[i].inputIndicesSizeAlignedTo8 = jointGroupSize.padded.cols - (jointGroupSize.padded.cols % 8u);
        offset += jointGroupSize.padded.cols;
    }
}

template<typename TValue>
void JointsBuilderCommon<TValue>::setOutputIndices(const dna::BehaviorReader* reader) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < reader->getJointGroupCount(); ++i) {
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        const auto indices = reader->getJointGroupOutputIndices(i);
        std::copy(indices.begin(), indices.end(), extd::advanced(storage.outputIndices.begin(), offset));
        storage.jointGroups[i].outputIndicesOffset = offset;
        offset += jointGroupSize.padded.rows;
    }
}

template<typename TValue>
void JointsBuilderCommon<TValue>::setLODs(const dna::BehaviorReader* reader) {
    std::uint32_t offset = 0ul;
    for (std::uint16_t i = 0u; i < reader->getJointGroupCount(); ++i) {
        const auto jointGroupSize = sizeReqs.getJointGroupSize(i);
        const auto dstRowCount = jointGroupSize.padded.rows;
        auto makeLODRegion = [dstRowCount, this](std::uint16_t lodRowCount) {
                return LODRegion{lodRowCount, dstRowCount, blockHeight, padTo};
            };
        const auto lods = reader->getJointGroupLODs(i);
        std::transform(lods.begin(), lods.end(), std::back_inserter(storage.lodRegions), makeLODRegion);
        storage.jointGroups[i].lodsOffset = offset;
        offset += reader->getLODCount();
    }
}

template<typename TValue>
typename JointsBuilderCommon<TValue>::JointsEvaluatorPtr JointsBuilderCommon<TValue>::build() {
    using ManagedJoints = pma::UniqueInstance<Evaluator<TValue>, JointsEvaluator>;
    return ManagedJoints::with(memRes).create(std::move(storage), std::move(strategy), memRes);
}

template class JointsBuilderCommon<float>;
template class JointsBuilderCommon<std::uint16_t>;

}  // namespace bpcm

}  // namespace rl4
