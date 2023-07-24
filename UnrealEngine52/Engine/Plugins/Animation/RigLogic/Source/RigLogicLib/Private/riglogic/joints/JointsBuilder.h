// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/riglogic/Configuration.h"

namespace rl4 {

struct RigMetrics;

class JointsBuilder {
    public:
        using Pointer = UniqueInstance<JointsBuilder>::PointerType;

    public:
        virtual ~JointsBuilder();

        static Pointer create(Configuration config, MemoryResource* memRes);

        virtual void computeStorageRequirements(const RigMetrics& source) = 0;
        virtual void computeStorageRequirements(const dna::BehaviorReader* source) = 0;
        virtual void allocateStorage(const dna::BehaviorReader* source) = 0;
        virtual void fillStorage(const dna::BehaviorReader* source) = 0;
        virtual JointsEvaluator::Pointer build() = 0;
};

}  // namespace rl4
