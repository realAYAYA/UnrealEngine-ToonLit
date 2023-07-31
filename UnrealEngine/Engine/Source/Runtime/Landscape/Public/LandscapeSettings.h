// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LandscapeSettings.generated.h"

class UMaterialInterface;
class ULandscapeLayerInfoObject;

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Landscape"))
class LANDSCAPE_API ULandscapeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Returns true if landscape resolution should be constrained. */
	bool IsLandscapeResolutionRestricted() const { return InRestrictiveMode(); }

	/** Returns true if blueprint landscape tools usage is allowed */
	bool AreBlueprintToolsAllowed() const { return !InRestrictiveMode(); }
	
	/** Returns the current landscape resolution limit. */
	int32 GetTotalResolutionLimit() const { return SideResolutionLimit * SideResolutionLimit; }

	bool InRestrictiveMode() const { return bRestrictiveMode; }
	void SetRestrictiveMode(bool bEnabled) { bRestrictiveMode = bEnabled; }

	int32 GetSideResolutionLimit() const { return SideResolutionLimit; }

	/** Returns the default landscape material that should be used when creating a new landscape. */
	TSoftObjectPtr<UMaterialInterface> GetDefaultLandscapeMaterial() const { return DefaultLandscapeMaterial; }

	/** Returns the default landscape layer info object that will be assigned to unset layers when creating a new landscape. */
	TSoftObjectPtr<ULandscapeLayerInfoObject> GetDefaultLayerInfoObject() const { return DefaultLayerInfoObject; }

public:
	UPROPERTY(config, EditAnywhere, Category = "Layers", meta=(UIMin = "1", UIMax = "32", ClampMin = "1", ClampMax = "32", ToolTip = "This option controls the maximum editing layers that can be added to a Landscape"))
	int32 MaxNumberOfLayers = 8;

protected:
	UPROPERTY(config)
	int32 SideResolutionLimit = 2048;

	UPROPERTY(EditAnywhere, config, Category = "Materials", meta = (ToolTip = "Default Landscape Material will be prefilled when creating a new landscape."))
	TSoftObjectPtr<UMaterialInterface> DefaultLandscapeMaterial;

	UPROPERTY(EditAnywhere, config, Category = "Layers", meta = (ToolTip = "Default Layer Info Object"))
	TSoftObjectPtr<ULandscapeLayerInfoObject> DefaultLayerInfoObject;

	UPROPERTY(transient)
	bool bRestrictiveMode = false;
};
