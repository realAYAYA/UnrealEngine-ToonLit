// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class DISPLAYCLUSTER_API IDisplayClusterViewport_CustomPostProcessSettings
{
public:
	virtual ~IDisplayClusterViewport_CustomPostProcessSettings() = default;

public:
	enum class ERenderPass : uint8
	{
		Start = 0,
		Override,
		Final,
		FinalPerViewport,
	};

public:
	// Override posproces, if defined
	// * return true, if processed
	virtual bool DoPostProcess(const ERenderPass InRenderPass, struct FPostProcessSettings* OutSettings, float* OutBlendWeight = nullptr) const = 0;
};
