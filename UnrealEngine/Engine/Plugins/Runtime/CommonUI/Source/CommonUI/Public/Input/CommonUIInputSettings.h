// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"
#include "UITag.h"
#include "CommonUIInputSettings.generated.h"

class UPlayerInput;

USTRUCT()
struct FUIActionKeyMapping
{
	GENERATED_BODY()

public:
	FUIActionKeyMapping() {}
	FUIActionKeyMapping(FKey InKey, float InHoldTime, float InHoldRollbackTime)
		: Key(InKey)
		, HoldTime(InHoldTime)
		, HoldRollbackTime(InHoldRollbackTime)
	{}

	/** A key that triggers this action */
	UPROPERTY(EditAnywhere, Config, Category = "UI Action Key Mapping")
	FKey Key;

	/** How long must the key be held down for the action to be executed? */
	UPROPERTY(EditAnywhere, Config, Category = "UI Action Key Mapping")
	float HoldTime = 0.f;
	
	/** Time (in seconds) for hold progress to go from 1.0 (completed) to 0.0. */
	UPROPERTY(EditAnywhere, Config, Category = "UI Action Key Mapping")
	float HoldRollbackTime = 0.f;
};

USTRUCT()
struct COMMONUI_API FUIInputAction
{
	GENERATED_BODY()
		
public:
	/** The UI.Action tag that acts as the universal identifier of this action. */
	UPROPERTY(EditAnywhere, Config, Category = "UI Input Action")
	FUIActionTag ActionTag;

	/** 
	 * Whenever a UI input action is bound, an override display name can optionally be provided.
	 * This is the default generic display name of this action for use in the absence of such an override.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "UI Input Action")
	FText DefaultDisplayName;

	/** All key mappings that will trigger this action */
	UPROPERTY(EditAnywhere, Config, Category = "UI Input Action", meta = (TitleProperty = "Key"))
	TArray<FUIActionKeyMapping> KeyMappings;

	bool HasAnyHoldMappings() const;
};

USTRUCT()
struct FCommonAnalogCursorSettings
{
	GENERATED_BODY();

public:
	/** The registration priority of the analog cursor preprocessor. */
	UPROPERTY(EditAnywhere, Config, Category = General)
	int32 PreprocessorPriority = 2;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor)
	bool bEnableCursorAcceleration = true;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor, meta = (EditCondition = bEnableAnalogCursorAcceleration))
	float CursorAcceleration = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor)
	float CursorMaxSpeed = 2200.f;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor, meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float CursorDeadZone = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoverSlowdownFactor = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor, meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ScrollDeadZone = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor)
	float ScrollUpdatePeriod = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = AnalogCursor)
	float ScrollMultiplier = 2.5f;

	FCommonAnalogCursorSettings() {}
};

/** Project-wide input settings for UI input actions */
UCLASS(config=Input, defaultconfig, MinimalAPI)
class UCommonUIInputSettings : public UObject
{
	GENERATED_BODY()

public:
	COMMONUI_API static const UCommonUIInputSettings& Get();
	
	virtual void PostInitProperties() override;

	int32 GetUIActionProcessingPriority() const { return UIActionProcessingPriority; }
	COMMONUI_API const FUIInputAction* FindAction(FUIActionTag ActionTag) const;
	COMMONUI_API const TArray<FUIInputAction>& GetUIInputActions() const { return InputActions; }
	const FCommonAnalogCursorSettings& GetAnalogCursorSettings() const { return AnalogCursorSettings; }
//
private:
	/** True to have the mouse pointer automatically moved to the center of whatever widget is currently focused while using a gamepad. */
	UPROPERTY(EditAnywhere, Config, Category = General)
	bool bLinkCursorToGamepadFocus = true;

	/** 
	 * The input priority of the input components that process UI input actions.
	 * The lower the value, the higher the priority of the component.
	 *
	 * By default, this value is incredibly high to ensure UI action processing priority over game elements.
	 * Adjust as needed for the UI input components to be processed at the appropriate point in the input stack in your project.
	 *
	 * NOTE: When the active input mode is ECommonInputMode::Menu, ALL input components with lower priority than this will be fully blocked.
	 *		 Thus, if any game agent input components are registered with higher priority than this, behavior will not match expectation.
	 */
	UPROPERTY(EditAnywhere, Config, Category = General)
	int32 UIActionProcessingPriority = 10000;

	/** All UI input action mappings for the project */
	UPROPERTY(EditAnywhere, Config, Category = Actions, meta = (TitleProperty = "ActionTag"))
	TArray<FUIInputAction> InputActions;

	/** Config-only set of input action overrides - if an entry for a given action is both here and in the InputActions array, this entry wins completely. */
	UPROPERTY(Config)
	TArray<FUIInputAction> ActionOverrides;

	UPROPERTY(EditAnywhere, Config, Category = AnalogCursor)
	FCommonAnalogCursorSettings AnalogCursorSettings;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Input/CommonUIInputTypes.h"
#include "UObject/Object.h"
#endif
