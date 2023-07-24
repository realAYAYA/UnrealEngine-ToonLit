// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpenXRInputSettings.generated.h"

/**
* Implements the settings for the OpenXR Input plugin.
*/
UCLASS(config = Input, defaultconfig)
class OPENXRINPUT_API UOpenXRInputSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Set a mappable input config to allow OpenXR runtimes to remap the Enhanced Input actions. */
	UPROPERTY(config, EditAnywhere, Category = "Enhanced Input", meta = (DisplayName = "Mappable Input Config for XR", AllowedClasses = "/Script/EnhancedInput.PlayerMappableInputConfig"))
	FSoftObjectPath MappableInputConfig = nullptr;

	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};
