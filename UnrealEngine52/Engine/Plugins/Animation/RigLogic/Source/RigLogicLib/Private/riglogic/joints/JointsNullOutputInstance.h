// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/JointsOutputInstance.h"

namespace rl4 {

class JointsNullOutputInstance : public JointsOutputInstance {
    public:
        ArrayView<float> getOutputBuffer() override;

};

}  // namespace rl4
