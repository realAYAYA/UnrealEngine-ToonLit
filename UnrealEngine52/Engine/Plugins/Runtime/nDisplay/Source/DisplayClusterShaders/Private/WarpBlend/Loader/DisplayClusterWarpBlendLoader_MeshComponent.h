// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WarpBlend/IDisplayClusterWarpBlendManager.h"

class IDisplayClusterWarpBlend;

class FDisplayClusterWarpBlendLoader_MeshComponent
{
public:
	static bool Load(const FDisplayClusterWarpBlendConstruct::FAssignWarpStaticMesh& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend);
};
