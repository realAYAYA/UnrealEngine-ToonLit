// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Delegates/Delegate.h"
#include "LandscapeSettings.generated.h"

class UMaterialInterface;
class ULandscapeLayerInfoObject;

DECLARE_DELEGATE_OneParam(FnOnMaxComponentsChanged, uint64);

UENUM()
enum class ELandscapeDirtyingMode : uint8
{
	Auto,
	InLandscapeModeOnly,
	InLandscapeModeAndUserTriggeredChanges
};

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Landscape"), MinimalAPI)
class ULandscapeSettings : public UDeveloperSettings
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

	UPROPERTY(config, EditAnywhere, Category = "Configuration", meta=(ToolTip = "Maximum Dimension of Landscape in Components"))
	int32 MaxComponents = 256;

	UPROPERTY(config, EditAnywhere, Category = "Configuration", meta = (UIMin = "1", UIMax = "1024", ClampMin = "1", ClampMax = "1024", ToolTip = "Maximum Size of Import Image Cache in MB"))
	uint32 MaxImageImportCacheSizeMegaBytes = 256;

	UPROPERTY(config, EditAnywhere, Category = "Configuration", meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Exponent for the Paint Tool Strength"))
	float PaintStrengthGamma = 2.2f;

	UPROPERTY(config, EditAnywhere, Category = "Configuration", meta = (ToolTip = "Disable Painting Startup Slowdown"))
	bool bDisablePaintingStartupSlowdown = true;
	
	UPROPERTY(Config, Category = "Configuration", EditAnywhere)
	ELandscapeDirtyingMode LandscapeDirtyingMode;

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
