// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/controls/Controls.h"

#include <pma/utils/ManagedInstance.h>

namespace dna {

class BehaviorReader;

}  // namespace dna

namespace rl4 {

struct ControlsFactory {
    using ManagedControls = pma::UniqueInstance<Controls>;
    using ControlsPtr = typename ManagedControls::PointerType;

    static ControlsPtr create(MemoryResource* memRes);
    static ControlsPtr create(const dna::BehaviorReader* reader, MemoryResource* memRes);
};

}  // namespace rl4
