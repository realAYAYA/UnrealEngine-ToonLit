// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#if WITH_EDITORONLY_DATA
#include "UObject/SoftObjectPtr.h"
#endif

#include "AvaMaskSettings.generated.h"

#if WITH_EDITORONLY_DATA
class UMaterialFunctionInterface;
#endif

/** Settings for Motion Design Mask */
UCLASS(Config = Engine, meta = (DisplayName = "Mask"))
class UAvaMaskSettings
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UAvaMaskSettings();

#if WITH_EDITORONLY_DATA
	/** Material Function to use to expect or add to a material. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, NoClear, Category = "Material")
	TSoftObjectPtr<UMaterialFunctionInterface> MaterialFunction;

	/** Get the user specified or default material function. */
	UMaterialFunctionInterface* GetMaterialFunction();
#endif

private:
#if WITH_EDITORONLY_DATA
	UMaterialFunctionInterface* GetDefaultMaterialFunction();
#endif
};
