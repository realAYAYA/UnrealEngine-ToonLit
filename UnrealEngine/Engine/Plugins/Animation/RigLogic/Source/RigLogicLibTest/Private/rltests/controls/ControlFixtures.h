// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/Controls.h"

#include <cstddef>

struct ControlsFactory {
    static rl4::ControlsInputInstance::Factory getInstanceFactory(std::uint16_t guiControlCount,
                                                                  std::uint16_t rawControlCount,
                                                                  std::uint16_t psdControlCount,
                                                                  std::uint16_t mlControlCount);
};
