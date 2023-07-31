// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/types/Aliases.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

class JointsBuilder {
    public:
        using JointsEvaluatorPtr = std::unique_ptr<JointsEvaluator, std::function<void (JointsEvaluator*)> >;
        using JointsBuilderPtr = std::unique_ptr<JointsBuilder, std::function<void (JointsBuilder*)> >;

    public:
        virtual ~JointsBuilder();

        static JointsBuilderPtr create(Configuration config, MemoryResource* memRes);

        virtual void allocateStorage(const dna::BehaviorReader* source) = 0;
        virtual void fillStorage(const dna::BehaviorReader* source) = 0;
        virtual JointsEvaluatorPtr build() = 0;
};

}  // namespace rl4
