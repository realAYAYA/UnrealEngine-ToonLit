// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/ml/MachineLearnedBehaviorNullOutputInstance.h"

namespace rl4 {

ArrayView<float> MachineLearnedBehaviorNullOutputInstance::getMaskBuffer() {
    return {};
}

ConstArrayView<float> MachineLearnedBehaviorNullOutputInstance::getMaskBuffer() const {
    return {};
}

}  // namespace rl4
