// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

namespace rl4 {

class MachineLearnedBehaviorOutputInstance {
    public:
        using Pointer = UniqueInstance<MachineLearnedBehaviorOutputInstance>::PointerType;

    public:
        virtual ~MachineLearnedBehaviorOutputInstance();
        virtual ArrayView<float> getMaskBuffer() = 0;
        virtual ConstArrayView<float> getMaskBuffer() const = 0;

};

}  // namespace rl4
