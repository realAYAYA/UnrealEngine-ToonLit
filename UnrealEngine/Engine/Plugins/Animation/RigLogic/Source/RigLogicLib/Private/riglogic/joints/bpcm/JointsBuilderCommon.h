// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/bpcm/JointStorage.h"
#include "riglogic/joints/bpcm/StorageSize.h"
#include "riglogic/joints/bpcm/JointCalculationStrategy.h"
#include "riglogic/types/Aliases.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace bpcm {

template<typename TValue>
class JointsBuilderCommon : public JointsBuilder {
    protected:
        using CalculationStrategy = JointCalculationStrategy<TValue>;
        using CalculationStrategyPtr = std::unique_ptr<CalculationStrategy, std::function<void (CalculationStrategy*)> >;

    protected:
        JointsBuilderCommon(std::uint32_t blockHeight_, std::uint32_t padTo_, MemoryResource* memRes_);

    public:
        void allocateStorage(const dna::BehaviorReader* reader) override;
        void fillStorage(const dna::BehaviorReader* reader) override;
        JointsEvaluatorPtr build() override;

    protected:
        virtual void setValues(const dna::BehaviorReader* source) = 0;
        virtual void setInputIndices(const dna::BehaviorReader* reader);
        virtual void setOutputIndices(const dna::BehaviorReader* reader);
        virtual void setLODs(const dna::BehaviorReader* reader);

    protected:
        std::uint32_t blockHeight;
        std::uint32_t padTo;
        MemoryResource* memRes;

        StorageSize sizeReqs;

        JointStorage<TValue> storage;
        CalculationStrategyPtr strategy;
};

}  // namespace bpcm

}  // namespace rl4
