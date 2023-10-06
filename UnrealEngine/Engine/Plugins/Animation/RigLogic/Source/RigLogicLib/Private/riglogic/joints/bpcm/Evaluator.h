// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/joints/JointsOutputInstance.h"
#include "riglogic/joints/bpcm/CalculationStrategy.h"
#include "riglogic/joints/bpcm/Storage.h"
#include "riglogic/joints/bpcm/StorageView.h"
#include "riglogic/riglogic/RigInstanceImpl.h"

#include <cstdint>

namespace rl4 {

namespace bpcm {

template<typename TValue>
class Evaluator : public JointsEvaluator {
    public:
        using CalculationStrategy = JointCalculationStrategy<TValue>;
        using CalculationStrategyPtr = typename UniqueInstance<CalculationStrategy>::PointerType;

        struct Accessor;
        friend Accessor;

    private:
        using JointGroupArrayView = typename CalculationStrategy::JointGroupArrayView;

    public:
        Evaluator(JointStorage<TValue>&& storage_,
                  CalculationStrategyPtr strategy_,
                  JointsOutputInstance::Factory instanceFactory_,
                  MemoryResource* memRes_) :
            memRes{memRes_},
            storage{std::move(storage_)},
            storageView{takeStorageSnapshot(storage, memRes)},
            jointGroupsView{storageView},
            strategy{std::move(strategy_)},
            instanceFactory{instanceFactory_} {
        }

        JointsOutputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const override {
            return instanceFactory(instanceMemRes);
        }

        void calculate(const ControlsInputInstance* inputs, JointsOutputInstance* outputs, std::uint16_t lod) const override {
            assert(strategy != nullptr);
            strategy->calculate(jointGroupsView, inputs->getInputBuffer(), outputs->getOutputBuffer(), lod);
        }

        void calculate(const ControlsInputInstance* inputs,
                       JointsOutputInstance* outputs,
                       std::uint16_t lod,
                       std::uint16_t jointGroupIndex) const override {
            assert(strategy != nullptr);
            strategy->calculate(jointGroupsView,
                                inputs->getInputBuffer(),
                                outputs->getOutputBuffer(),
                                lod,
                                jointGroupIndex);
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
        JointsOutputInstance::Factory instanceFactory;
};

}  // namespace bpcm

}  // namespace rl4
