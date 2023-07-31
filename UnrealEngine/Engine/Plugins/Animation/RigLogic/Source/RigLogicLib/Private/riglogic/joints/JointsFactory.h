// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/Joints.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigMetrics.h"
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

struct JointsFactory {
    using JointsPtr = std::unique_ptr<Joints, std::function<void (Joints*)> >;

    static JointsPtr create(const Configuration& config, const dna::BehaviorReader* reader, MemoryResource* memRes);
    static JointsPtr create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes);

};

}  // namespace rl4
