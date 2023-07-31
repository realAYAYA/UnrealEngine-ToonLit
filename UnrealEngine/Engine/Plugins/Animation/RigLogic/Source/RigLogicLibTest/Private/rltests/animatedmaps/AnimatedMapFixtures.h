// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/animatedmaps/AnimatedMaps.h"
#include "riglogic/conditionaltable/ConditionalTable.h"

#include <cstddef>

struct AnimatedMapsFactory {

    static rl4::AnimatedMaps withDefaults(rl4::ConditionalTable&& conditionals, rl4::MemoryResource* memRes);

};
