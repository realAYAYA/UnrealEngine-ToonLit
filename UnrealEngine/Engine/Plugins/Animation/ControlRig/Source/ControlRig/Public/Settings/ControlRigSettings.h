// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ControlRigSettings.h: Declares the ControlRigSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ControlRigGizmoLibrary.h"

#if WITH_EDITOR
#include "RigVMModel/RigVMGraph.h"
#endif

#include "ControlRigSettings.generated.h"

class UStaticMesh;

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
#endif
	
	static UControlRigSettings * Get() { return GetMutableDefault<UControlRigSettings>(); }
};

/**
 * Customize Control Rig Editor.
 */
UCLASS(config = EditorPerProjectUserSettings, meta=(DisplayName="Control Rig Editor"))
class CONTROLRIG_API UControlRigEditorSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return TEXT("ContentEditors"); }
	
#if WITH_EDITORONLY_DATA

	// When this is checked mutable nodes (nodes with an execute pin)
	// will be hooked up automatically.
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bAutoLinkMutableNodes;

	// When this is checked all controls will return to their initial
	// value as the user hits the Compile button.
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bResetControlsOnCompile;

	// When this is checked all controls will return to their initial
	// value as the user interacts with a pin value
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bResetControlsOnPinValueInteraction;

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
	 * The default node snippet to create when pressing 1 + Left Mouse Button
	 */
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "1"))
	FString NodeSnippet_1;

	/**
	* The default node snippet to create when pressing 2 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "2"))
	FString NodeSnippet_2;

	/**
	* The default node snippet to create when pressing 3 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "3"))
	FString NodeSnippet_3;

	/**
	* The default node snippet to create when pressing 4 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "4"))
	FString NodeSnippet_4;

	/**
	* The default node snippet to create when pressing 5 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "5"))
	FString NodeSnippet_5;

	/**
	* The default node snippet to create when pressing 6 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "6"))
	FString NodeSnippet_6;

	/**
	* The default node snippet to create when pressing 7 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "7"))
	FString NodeSnippet_7;

	/**
	* The default node snippet to create when pressing 8 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "8"))
	FString NodeSnippet_8;

	/**
	* The default node snippet to create when pressing 9 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "9"))
	FString NodeSnippet_9;

	/**
	* The default node snippet to create when pressing 0 + Left Mouse Button
	*/
	UPROPERTY(EditAnywhere, config, Category = Snippets, meta = (DisplayName = "0"))
	FString NodeSnippet_0;

#endif

	static UControlRigEditorSettings * Get() { return GetMutableDefault<UControlRigEditorSettings>(); }
};


