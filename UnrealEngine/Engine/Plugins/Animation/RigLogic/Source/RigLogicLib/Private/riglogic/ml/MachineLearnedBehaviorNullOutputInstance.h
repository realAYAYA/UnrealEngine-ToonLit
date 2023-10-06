// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/ml/MachineLearnedBehaviorOutputInstance.h"

namespace rl4 {

class MachineLearnedBehaviorNullOutputInstance : public MachineLearnedBehaviorOutputInstance {
    public:
        ArrayView<float> getMaskBuffer() override;
        ConstArrayView<float> getMaskBuffer() const override;

};

}  // namespace rl4
