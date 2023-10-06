// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/joints/JointsNullOutputInstance.h"

#include <cstdint>

namespace rl4 {

class ControlsInputInstance;

class JointsNullEvaluator : public JointsEvaluator {
    public:
        JointsOutputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const override;
        void calculate(const ControlsInputInstance*  /*unused*/, JointsOutputInstance*  /*unused*/,
                       std::uint16_t  /*unused*/) const override;
        void calculate(const ControlsInputInstance*  /*unused*/,
                       JointsOutputInstance*  /*unused*/,
                       std::uint16_t  /*unused*/,
                       std::uint16_t  /*unused*/) const override;
        void load(terse::BinaryInputArchive<BoundedIOStream>&  /*unused*/) override;
        void save(terse::BinaryOutputArchive<BoundedIOStream>&  /*unused*/) override;

};

}  // namespace rl4
