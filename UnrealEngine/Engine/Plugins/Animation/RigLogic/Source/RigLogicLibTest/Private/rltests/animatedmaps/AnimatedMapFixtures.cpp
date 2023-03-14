// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/animatedmaps/AnimatedMapFixtures.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/animatedmaps/AnimatedMaps.h"
#include "riglogic/conditionaltable/ConditionalTable.h"

namespace defaults {

const std::size_t lodCount = 1ul;
const std::uint16_t lods[lodCount] = {2};

}  // namespace calculation

rl4::AnimatedMaps AnimatedMapsFactory::withDefaults(rl4::ConditionalTable&& conditionals, rl4::MemoryResource* memRes) {
    return rl4::AnimatedMaps{
        rl4::Vector<std::uint16_t>{defaults::lods, defaults::lods + defaults::lodCount, memRes},
        std::move(conditionals)
    };
}
