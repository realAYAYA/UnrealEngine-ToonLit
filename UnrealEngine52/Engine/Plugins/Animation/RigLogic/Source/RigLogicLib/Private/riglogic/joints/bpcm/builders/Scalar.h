// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/BuilderCommon.h"
#include "riglogic/joints/bpcm/Consts.h"
#include "riglogic/joints/bpcm/strategies/Scalar.h"
#include "riglogic/types/Aliases.h"

namespace rl4 {

namespace bpcm {

class ScalarJointsBuilder : public JointsBuilderCommon<float, block8Height, block4Height, trimd::scalar::F128>  {
    private:
        using Super = JointsBuilderCommon<float, block8Height, block4Height, trimd::scalar::F128>;

    public:
        explicit ScalarJointsBuilder(MemoryResource* memRes_) : Super{UniqueInstance<ScalarJointCalculationStrategy,
                                                                                     CalculationStrategy>::with(
                                                                          memRes_).create(), memRes_} {
        }

        ~ScalarJointsBuilder();

};

}  // namespace bpcm

}  // namespace rl4
