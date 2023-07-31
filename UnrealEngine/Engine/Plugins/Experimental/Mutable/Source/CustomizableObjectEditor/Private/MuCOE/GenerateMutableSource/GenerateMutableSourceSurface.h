// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeComponentNew.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;
struct FMutableGraphSurfaceGenerationData;

mu::NodeSurfacePtr GenerateMutableSourceSurface(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext, FMutableGraphSurfaceGenerationData& SurfaceData);
