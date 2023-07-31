// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingOutputComponent.h"
#include "DMXPixelMappingOutputDMXComponent.generated.h"


/**
 * Parent class for DMX sending components
 */
UCLASS(Abstract)
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingOutputDMXComponent
	: public UDMXPixelMappingOutputComponent
{
	GENERATED_BODY()

public:
	/** Render input texture for downsample texture, donwsample and send DMX for this component */
	UFUNCTION(BlueprintCallable, Category = "DMX|PixelMapping")
	virtual void RenderWithInputAndSendDMX() PURE_VIRTUAL(UDMXPixelMappingOutputComponent::RenderWithInputAndSendDMX);

	/** Check if a Component can be moved under another one (used for copy/move/duplicate) */
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override
	{
		return false;
	}
};
