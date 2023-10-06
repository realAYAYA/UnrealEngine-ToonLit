// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/Controls.h"

namespace rl4 {

struct Configuration;
struct RigMetrics;

struct ControlsFactory {
    static Controls::Pointer create(const Configuration& config, const dna::Reader* reader, MemoryResource* memRes);
    static Controls::Pointer create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes);

};

}  // namespace rl4
