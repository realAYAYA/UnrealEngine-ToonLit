// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/ml/MachineLearnedBehaviorFactory.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/utils/Extd.h"
#include "riglogic/ml/MachineLearnedBehaviorEvaluator.h"
#include "riglogic/ml/MachineLearnedBehaviorNullEvaluator.h"
#include "riglogic/ml/cpu/Factory.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigMetrics.h"
#include "riglogic/system/simd/Detect.h"
#include "riglogic/system/simd/SIMD.h"

#include <cstdint>

namespace rl4 {

#ifdef RL_USE_HALF_FLOATS
    using StorageValueType = std::uint16_t;
#else
    using StorageValueType = float;
#endif  // RL_USE_HALF_FLOATS

MachineLearnedBehaviorEvaluator::Pointer createEvaluator(const Configuration& config,
                                                         const dna::MachineLearnedBehaviorReader* reader,
                                                         MemoryResource* memRes) {
    // Work around unused parameter warning when building without SSE and AVX
    static_cast<void>(config);
    #ifdef RL_BUILD_WITH_SSE
        if (config.calculationType == CalculationType::SSE) {
            return ml::cpu::Factory<StorageValueType, trimd::sse::F256, trimd::sse::F128>::create(reader, memRes);
        }
    #endif  // RL_BUILD_WITH_SSE
    #ifdef RL_BUILD_WITH_AVX
        if (config.calculationType == CalculationType::AVX) {
            // Use 256-bit AVX registers and whatever 128-bit width type is available
            return ml::cpu::Factory<StorageValueType, trimd::avx::F256, trimd::F128>::create(reader, memRes);
        }
    #endif  // RL_BUILD_WITH_AVX
    return ml::cpu::Factory<float, trimd::scalar::F256, trimd::scalar::F128>::create(reader, memRes);
}

MachineLearnedBehavior::Pointer MachineLearnedBehaviorFactory::create(const Configuration& config,
                                                                      const dna::MachineLearnedBehaviorReader* reader,
                                                                      MemoryResource* memRes) {
    auto moduleFactory = UniqueInstance<MachineLearnedBehavior>::with(memRes);
    if (!config.loadMachineLearnedBehavior || (reader->getNeuralNetworkCount() == 0u)) {
        auto evaluator =
            UniqueInstance<MachineLearnedBehaviorNullEvaluator, MachineLearnedBehaviorEvaluator>::with(memRes).create();
        return moduleFactory.create(std::move(evaluator), memRes);
    }

    Vector<Matrix<std::uint16_t> > neuralNetworkIndicesPerMeshRegion{memRes};
    neuralNetworkIndicesPerMeshRegion.resize(reader->getMeshCount());
    for (std::uint16_t meshIdx = {}; meshIdx < neuralNetworkIndicesPerMeshRegion.size(); ++meshIdx) {
        neuralNetworkIndicesPerMeshRegion[meshIdx].resize(reader->getMeshRegionCount(meshIdx));
        for (std::uint16_t regionIdx = {}; regionIdx < neuralNetworkIndicesPerMeshRegion[meshIdx].size(); ++regionIdx) {
            const auto netIndices = reader->getNeuralNetworkIndicesForMeshRegion(meshIdx, regionIdx);
            neuralNetworkIndicesPerMeshRegion[meshIdx][regionIdx].assign(netIndices.begin(), netIndices.end());
        }
    }

    return moduleFactory.create(createEvaluator(config, reader, memRes), std::move(neuralNetworkIndicesPerMeshRegion));
}

MachineLearnedBehavior::Pointer MachineLearnedBehaviorFactory::create(const Configuration& config, MemoryResource* memRes) {
    auto moduleFactory = UniqueInstance<MachineLearnedBehavior>::with(memRes);
    if (!config.loadMachineLearnedBehavior) {
        auto evaluator =
            UniqueInstance<MachineLearnedBehaviorNullEvaluator, MachineLearnedBehaviorEvaluator>::with(memRes).create();
        return moduleFactory.create(std::move(evaluator), memRes);
    }
    return moduleFactory.create(createEvaluator(config, nullptr, memRes), memRes);
}

}  // namespace rl4
