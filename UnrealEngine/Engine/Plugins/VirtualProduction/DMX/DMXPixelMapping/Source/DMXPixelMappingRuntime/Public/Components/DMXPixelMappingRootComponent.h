// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingBaseComponent.h"
#include "DMXPixelMappingRootComponent.generated.h"

/**
 * Root component in the components tree
 */
UCLASS()
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingRootComponent
	: public UDMXPixelMappingBaseComponent
{
	GENERATED_BODY()

protected:
	// ~Begin UObject Interface
	virtual void BeginDestroy() override;
	// ~End UObject Interface

public:
	//~ Begin UDMXPixelMappingBaseComponent implementation
	virtual const FName& GetNamePrefix() override;
	virtual void ResetDMX() override;
	virtual void SendDMX() override;
	virtual void Render() override;
	virtual void RenderAndSendDMX() override;
	//~ End UDMXPixelMappingBaseComponent implementation

	/** Check if a Component can be moved under another one (used for copy/move/duplicate) */
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override;
};
