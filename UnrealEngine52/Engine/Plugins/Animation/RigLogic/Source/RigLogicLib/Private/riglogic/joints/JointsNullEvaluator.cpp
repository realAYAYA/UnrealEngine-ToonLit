// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/JointsNullEvaluator.h"

namespace rl4 {

JointsOutputInstance::Pointer JointsNullEvaluator::createInstance(MemoryResource* instanceMemRes) const {
    return UniqueInstance<JointsNullOutputInstance, JointsOutputInstance>::with(instanceMemRes).create();
}

void JointsNullEvaluator::calculate(const ControlsInputInstance*  /*unused*/, JointsOutputInstance*  /*unused*/,
                                    std::uint16_t  /*unused*/) const {
}

void JointsNullEvaluator::calculate(const ControlsInputInstance*  /*unused*/,
                                    JointsOutputInstance*  /*unused*/,
                                    std::uint16_t  /*unused*/,
                                    std::uint16_t  /*unused*/) const {
}

void JointsNullEvaluator::load(terse::BinaryInputArchive<BoundedIOStream>&  /*unused*/) {
}

void JointsNullEvaluator::save(terse::BinaryOutputArchive<BoundedIOStream>&  /*unused*/) {
}

}  // namespace rl4
