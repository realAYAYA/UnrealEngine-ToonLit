// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DebugCameraControllerSettings.h: Declares the DebugCameraControllerSettings class.
=============================================================================*/

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "DebugCameraControllerSettings.generated.h"

USTRUCT()
struct ENGINE_API FDebugCameraControllerSettingsViewModeIndex
{
	GENERATED_USTRUCT_BODY()

	FDebugCameraControllerSettingsViewModeIndex() { ViewModeIndex = VMI_BrushWireframe; }
	FDebugCameraControllerSettingsViewModeIndex(EViewModeIndex InViewModeIndex) : ViewModeIndex(InViewModeIndex) {}

	virtual ~FDebugCameraControllerSettingsViewModeIndex() { }

	UPROPERTY(EditAnywhere, Category = General, meta = (DisplayName = "Cycle View Mode"))
	TEnumAsByte<EViewModeIndex> ViewModeIndex;
};

/**
 * Default debug camera controller settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Debug Camera Controller"))
class ENGINE_API UDebugCameraControllerSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category = General)
	TArray<FDebugCameraControllerSettingsViewModeIndex> CycleViewModes;

public:
#if WITH_EDITOR

	// UObject interface.
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	void RemoveInvalidViewModes();

#endif // WITH_EDITOR

public:
	TArray<EViewModeIndex> GetCycleViewModes();

	static UDebugCameraControllerSettings * Get() { return CastChecked<UDebugCameraControllerSettings>(UDebugCameraControllerSettings::StaticClass()->GetDefaultObject()); }
};
