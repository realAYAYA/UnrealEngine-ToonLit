// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/ml/MachineLearnedBehaviorNullEvaluator.h"

#include "riglogic/ml/MachineLearnedBehaviorNullOutputInstance.h"

namespace rl4 {

MachineLearnedBehaviorOutputInstance::Pointer MachineLearnedBehaviorNullEvaluator::createInstance(MemoryResource* instanceMemRes)
const {
    return UniqueInstance<MachineLearnedBehaviorNullOutputInstance,
                          MachineLearnedBehaviorOutputInstance>::with(instanceMemRes).create();
}

void MachineLearnedBehaviorNullEvaluator::calculate(ControlsInputInstance*  /*unused*/,
                                                    MachineLearnedBehaviorOutputInstance*  /*unused*/,
                                                    std::uint16_t  /*unused*/) const {
}

void MachineLearnedBehaviorNullEvaluator::calculate(ControlsInputInstance*  /*unused*/,
                                                    MachineLearnedBehaviorOutputInstance*  /*unused*/,
                                                    std::uint16_t  /*unused*/,
                                                    std::uint16_t  /*unused*/) const {
}

void MachineLearnedBehaviorNullEvaluator::load(terse::BinaryInputArchive<BoundedIOStream>&  /*unused*/) {
}

void MachineLearnedBehaviorNullEvaluator::save(terse::BinaryOutputArchive<BoundedIOStream>&  /*unused*/) {
}

}  // namespace rl4
