// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WarpBlend/IDisplayClusterWarpBlendManager.h"

class IDisplayClusterWarpBlend;

class FDisplayClusterWarpBlendLoader_MPCDI
{
public:
	static bool Load(const FDisplayClusterWarpBlendConstruct::FLoadMPCDIFile& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend);
	static bool Load(const FDisplayClusterWarpBlendConstruct::FLoadPFMFile& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend);
};
