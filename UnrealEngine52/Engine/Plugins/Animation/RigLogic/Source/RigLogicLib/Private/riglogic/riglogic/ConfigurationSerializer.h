// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/riglogic/Configuration.h"

namespace rl4 {

template<class Archive>
void serialize(Archive& archive, Configuration& config) {
    archive(config.calculationType,
            config.loadJoints,
            config.loadBlendShapes,
            config.loadAnimatedMaps,
            config.loadMachineLearnedBehavior);
}

}  // namespace rl4
