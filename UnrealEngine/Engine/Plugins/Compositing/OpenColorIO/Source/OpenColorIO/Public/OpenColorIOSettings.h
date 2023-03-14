// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "OpenColorIOSettings.generated.h"

/**
 * Rendering settings.
 */
UCLASS(config = Engine, defaultconfig)
class OPENCOLORIO_API UOpenColorIOSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOpenColorIOSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FName GetSectionName() const override;
#endif
	//~ End UDeveloperSettings interface

public:

	UPROPERTY(config, EditAnywhere, Category = Transform, meta = (
		DisplayName = "Enable Legacy Gpu Processor",
		ToolTip = "Whether to enable OCIO V1's legacy gpu processor.",
		ConfigRestartRequired = true))
	uint8 bUseLegacyProcessor : 1;

	UPROPERTY(config, EditAnywhere, Category = Transform, meta = (
		DisplayName = "32-bit float LUTs",
		ToolTip = "Whether to create lookup table texture resources in 32-bit float format (higher performance requirements).",
		ConfigRestartRequired = true))
	uint8 bUse32fLUT : 1;
};
