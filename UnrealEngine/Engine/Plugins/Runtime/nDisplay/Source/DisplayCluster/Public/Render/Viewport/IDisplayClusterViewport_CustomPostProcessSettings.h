// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* DC Viewport Postprocess interface
*/
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

	/** Add custom postprocess. */
	virtual void AddCustomPostProcess(const ERenderPass InRenderPass, const FPostProcessSettings& InSettings, float BlendWeight, bool bSingleFrame) = 0;

	/** remove custom postprocess. */
	virtual void RemoveCustomPostProcess(const ERenderPass InRenderPass) = 0;
};
