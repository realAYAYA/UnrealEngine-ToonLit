// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ControlRigSettings.h: Declares the ControlRigSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RigVMSettings.h"
#include "ControlRigGizmoLibrary.h"

#if WITH_EDITOR
#include "RigVMModel/RigVMGraph.h"
#endif

#include "ControlRigSettings.generated.h"

class UStaticMesh;
class UControlRig;

USTRUCT()
struct FControlRigSettingsPerPinBool
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category = Settings)
	TMap<FString, bool> Values;
};

/**
 * Default ControlRig settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Control Rig"))
class CONTROLRIG_API UControlRigSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, config, Category = Shapes)
	TSoftObjectPtr<UControlRigShapeLibrary> DefaultShapeLibrary;

	UPROPERTY(EditAnywhere, config, Category = ModularRigging, meta=(AllowedClasses="/Script/ControlRigDeveloper.ControlRigBlueprint"))
	FSoftObjectPath DefaultRootModule;
#endif
	
	static UControlRigSettings * Get() { return GetMutableDefault<UControlRigSettings>(); }
};

/**
 * Customize Control Rig Editor.
 */
UCLASS(config = EditorPerProjectUserSettings, meta=(DisplayName="Control Rig Editor"))
class CONTROLRIG_API UControlRigEditorSettings : public URigVMEditorSettings
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA

	// When this is checked all controls will return to their initial
	// value as the user hits the Compile button.
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bResetControlsOnCompile;

	// When this is checked all controls will return to their initial
	// value as the user interacts with a pin value
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bResetControlsOnPinValueInteraction;
	
	// When this is checked all elements will be reset to their initial value
	// if the user changes the event queue (for example between forward / backward solve)
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bResetPoseWhenTogglingEventQueue;

	// When this is checked any hierarchy interaction within the Control Rig
	// Editor will be stored on the undo stack
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bEnableUndoForPoseInteraction;

	/**
	 * When checked controls will be reset during a manual compilation
	 * (when pressing the Compile button)
	 */
	UPROPERTY(EditAnywhere, config, Category = Compilation)
	bool bResetControlTransformsOnCompile;

	/**
	 * A map which remembers the expansion setting for each rig unit pin.
	 */
	UPROPERTY(EditAnywhere, config, Category = NodeGraph)
	TMap<FString, FControlRigSettingsPerPinBool> RigUnitPinExpansion;
	
	/**
	 * The border color of the viewport when entering "Construction Event" mode
	 */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	FLinearColor ConstructionEventBorderColor;
	
	/**
	 * The border color of the viewport when entering "Backwards Solve" mode
	 */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	FLinearColor BackwardsSolveBorderColor;
	
	/**
	 * The border color of the viewport when entering "Backwards And Forwards" mode
	 */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	FLinearColor BackwardsAndForwardsBorderColor;

	/**
	 * Option to toggle displaying the stacked hierarchy items.
	 * Note that changing this option potentially requires to re-open the editors in question. 
	 */
	UPROPERTY(EditAnywhere, config, Category = Hierarchy)
	bool bShowStackedHierarchy;

	/**
 	 * The maximum number of stacked items in the view 
 	 * Note that changing this option potentially requires to re-open the editors in question. 
 	 */
	UPROPERTY(EditAnywhere, config, Category = Hierarchy, meta = (EditCondition = "bShowStackedHierarchy"))
	int32 MaxStackSize;

	/**
	 * If turned on we'll offer box / marquee selection in the control rig editor viewport.
	 */
	UPROPERTY(EditAnywhere, config, Category = Hierarchy)
	bool bLeftMouseDragDoesMarquee;

#endif

	static UControlRigEditorSettings * Get() { return GetMutableDefault<UControlRigEditorSettings>(); }
};


