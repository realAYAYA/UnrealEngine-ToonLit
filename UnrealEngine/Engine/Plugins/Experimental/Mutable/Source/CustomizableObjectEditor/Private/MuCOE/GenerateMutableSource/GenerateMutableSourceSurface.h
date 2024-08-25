// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeComponentNew.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;

mu::NodeSurfacePtr GenerateMutableSourceSurface(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext);
