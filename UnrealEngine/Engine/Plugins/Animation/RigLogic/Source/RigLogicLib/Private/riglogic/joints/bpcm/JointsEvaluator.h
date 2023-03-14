// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/joints/bpcm/JointCalculationStrategy.h"
#include "riglogic/joints/bpcm/JointStorage.h"
#include "riglogic/joints/bpcm/JointStorageView.h"
#include "riglogic/riglogic/RigInstanceImpl.h"

#include <terse/archives/binary/InputArchive.h>
#include <terse/archives/binary/OutputArchive.h>
#include <trio/Stream.h>

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

namespace bpcm {

template<typename TValue>
class Evaluator : public JointsEvaluator {
    public:
        using CalculationStrategy = JointCalculationStrategy<TValue>;
        using CalculationStrategyPtr = std::unique_ptr<CalculationStrategy, std::function<void (CalculationStrategy*)> >;

        struct Accessor;
        friend Accessor;

    private:
        using JointGroupArrayView = typename CalculationStrategy::JointGroupArrayView;

    public:
        Evaluator(JointStorage<TValue>&& storage_, CalculationStrategyPtr strategy_, MemoryResource* memRes_) :
            memRes{memRes_},
            storage{std::move(storage_)},
            storageView{takeStorageSnapshot(storage, memRes)},
            jointGroupsView{storageView},
            strategy{std::move(strategy_)} {
        }

        void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const override {
            assert(strategy != nullptr);
            strategy->calculate(jointGroupsView, inputs, outputs, lod);
        }

        void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod,
                       std::uint16_t jointGroupIndex) const override {
            assert(strategy != nullptr);
            strategy->calculate(jointGroupsView, inputs, outputs, lod, jointGroupIndex);
        }

        void load(terse::BinaryInputArchive<BoundedIOStream>& archive) override {
            archive(storage);
            storageView = takeStorageSnapshot(storage, memRes);
            jointGroupsView = JointGroupArrayView{storageView};
        }

        void save(terse::BinaryOutputArchive<BoundedIOStream>& archive) override {
            archive(storage);
        }

    private:
        MemoryResource* memRes;
        JointStorage<TValue> storage;
        Vector<JointGroupView<TValue> > storageView;
        JointGroupArrayView jointGroupsView;
        CalculationStrategyPtr strategy;
};

}  // namespace bpcm

}  // namespace rl4
