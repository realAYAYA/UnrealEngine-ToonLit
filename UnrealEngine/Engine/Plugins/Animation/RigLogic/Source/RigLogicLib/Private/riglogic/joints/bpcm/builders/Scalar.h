// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/builders/Float.h"
#include "riglogic/types/Aliases.h"

namespace rl4 {

namespace bpcm {

class ScalarJointsBuilder : public FloatStorageBuilder {
    public:
        explicit ScalarJointsBuilder(MemoryResource* memRes_);
        ~ScalarJointsBuilder();
};

}  // namespace bpcm

}  // namespace rl4
