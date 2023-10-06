// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/MachineLearnedBehavior.h"

namespace rl4 {

struct Configuration;
struct RigMetrics;

struct MachineLearnedBehaviorFactory {
    static MachineLearnedBehavior::Pointer create(const Configuration& config,
                                                  const dna::MachineLearnedBehaviorReader* reader,
                                                  MemoryResource* memRes);
    static MachineLearnedBehavior::Pointer create(const Configuration& config, MemoryResource* memRes);

};

}  // namespace rl4
