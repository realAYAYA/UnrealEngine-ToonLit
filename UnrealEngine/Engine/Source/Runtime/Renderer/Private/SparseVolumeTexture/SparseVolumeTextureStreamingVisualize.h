// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ScreenPass.h"

class FRDGBuilder;

namespace UE
{
namespace SVT
{
	void AddStreamingDebugPass(FRDGBuilder& GraphBuilder, const FSceneView& View, FScreenPassTexture Output);
}
}