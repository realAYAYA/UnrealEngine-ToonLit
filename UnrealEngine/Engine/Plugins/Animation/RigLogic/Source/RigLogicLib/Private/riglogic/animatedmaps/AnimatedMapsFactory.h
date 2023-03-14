// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/animatedmaps/AnimatedMaps.h"

#include <pma/utils/ManagedInstance.h>

namespace dna {

class BehaviorReader;

}  // namespace dna

namespace rl4 {

struct AnimatedMapsFactory {
    using ManagedAnimatedMaps = pma::UniqueInstance<AnimatedMaps>;
    using AnimatedMapsPtr = typename ManagedAnimatedMaps::PointerType;

    static AnimatedMapsPtr create(MemoryResource* memRes);
    static AnimatedMapsPtr create(const dna::BehaviorReader* reader, MemoryResource* memRes);
};

}  // namespace rl4
