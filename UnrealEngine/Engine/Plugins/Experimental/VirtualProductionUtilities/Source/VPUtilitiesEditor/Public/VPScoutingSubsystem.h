// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "EditorUtilityWidget.h"
#include "Templates/SubclassOf.h"
#include "TickableEditorObject.h"
#include "UI/VREditorFloatingUI.h"
#include "UnrealEdMisc.h"
#include "VPScoutingSubsystem.generated.h"


UENUM(BlueprintType)
enum class EVProdPanelIDs : uint8
{
	Main,
	Left,
	Right,
	Context,
	Timeline,
	Measure,
	Gaffer
};


/*
 * Base class of the helper class defined in BP
 */
UCLASS(Abstract, Blueprintable, MinimalAPI, meta = (ShowWorldContextPin))
class UVPScoutingSubsystemHelpersBase : public UObject
{
	GENERATED_BODY()
};


/*
 * Base class of the gesture manager defined in BP
 */
UCLASS(Abstract, Blueprintable, MinimalAPI, meta = (ShowWorldContextPin))
class UVPScoutingSubsystemGestureManagerBase : public UObject, public FTickableEditorObject
{
	GENERATED_BODY()

public:
	UVPScoutingSubsystemGestureManagerBase();

	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "Tick")
	void EditorTick(float DeltaSeconds);

	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "VR")
	void OnVREditingModeEnter();
	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "VR")
	void OnVREditingModeExit();

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

	//~ Begin FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject interface

private:
	void OnVREditingModeEnterCallback();
	void OnVREditingModeExitCallback();
};


/*
 * Subsystem used for VR Scouting
 */
UCLASS()
class UVPScoutingSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UVPScoutingSubsystem();
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Subsystems can't have any Blueprint implementations, so we attach this class for any BP logic that we to provide. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Virtual Production")
	TObjectPtr<UVPScoutingSubsystemHelpersBase> VPSubsystemHelpers;

	/** GestureManager that manage some user input in VR editor. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Virtual Production")
	TObjectPtr<UVPScoutingSubsystemGestureManagerBase> GestureManager;

	/** bool to keep track of whether the settings menu panel in the main menu is open*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Menu")
	bool IsSettingsMenuOpen;

	/** This is a multiplier for grip nav speed so we can keep the grip nav value in the range 0-1 and increase this variable if we need a bigger range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Virtual Production")
	float GripNavSpeedCoeff;

	// @todo: Guard against user-created name collisions
	/** Open a widget UI in front of the user. Opens default VProd UI (defined via the 'Virtual Scouting User Interface' setting) if null. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	void ToggleVRScoutingUI(UPARAM(ref) FVREditorFloatingUICreationContext& CreationContext);

	/** Hide VR Sequencer Window*/
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	void HideInfoDisplayPanel();

	/** Check whether a widget UI is open*/
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	bool IsVRScoutingUIOpen(const FName& PanelID);

	/** Get UI panel Actor from the passed ID */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	AVREditorFloatingUI* GetPanelActor(const FName& PanelID) const;

	/** Get UI panel widget from the passed ID */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	UUserWidget* GetPanelWidget(const FName& PanelID) const;

	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static TArray<UVREditorInteractor*> GetActiveEditorVRControllers();

	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static const FName GetVProdPanelID(const EVProdPanelIDs Panel)
	{
		switch (Panel)
		{
			case EVProdPanelIDs::Main:
				return VProdPanelID;
			case EVProdPanelIDs::Right:
				return VProdPanelRightID;
			case EVProdPanelIDs::Left:
				return VProdPanelLeftID;
			case EVProdPanelIDs::Context:
				return VProdPanelContextID;
			case EVProdPanelIDs::Timeline:
				return VProdPanelTimelineID;
			case EVProdPanelIDs::Measure:
				return VProdPanelMeasureID;
			case EVProdPanelIDs::Gaffer:
				return VProdPanelGafferID;
		}

		return VProdPanelID;
	};

	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static FString GetDirectorName();

	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static FString GetShowName();

	/** Whether the VR user wants to use the metric system instead of imperial */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static bool IsUsingMetricSystem();

	/** Set whether the VR user wants to use the metric system instead of imperial */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetIsUsingMetricSystem(const bool bInUseMetricSystem);
	
	/** Whether the VR user wants to have the transform gizmo enabled */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static bool IsUsingTransformGizmo();

	/** Set whether the VR user wants to have the transform gizmo enabled */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetIsUsingTransformGizmo(const bool bInIsUsingTransformGizmo);

	/** Set value of cvar "VI.ShowTransformGizmo" */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetShowTransformGizmoCVar(const bool bInShowTransformGizmoCVar);

	/** Get flight speed for scouting in VR */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static float GetFlightSpeed();

	/** Set flight speed for scouting in VR */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetFlightSpeed(const float InFlightSpeed);

	/** Get grip nav speed for scouting in VR */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static float GetGripNavSpeed();

	/** Set grip nav speed for scouting in VR */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetGripNavSpeed(const float InGripNavSpeed);

	/** Whether grip nav inertia is enabled when scouting in VR */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static bool IsUsingInertiaDamping();

	/** Set whether grip nav inertia is enabled when scouting in VR */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetIsUsingInertiaDamping(const bool bInIsUsingInertiaDamping);

	/** Set value of cvar "VI.HighSpeedInertiaDamping" */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetInertiaDampingCVar(const float InInertiaDamping);

	/** Whether the helper system on the controllers is enabled */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static bool IsHelperSystemEnabled();

	/** Set whether the helper system on the controllers is enabled   */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void SetIsHelperSystemEnabled(const bool bInIsHelperSystemEnabled);

	/** Get VR Editor Mode object */
	UFUNCTION(BlueprintPure, Category = "Virtual Production")
	static UVREditorMode* GetVREditorMode();

	/** Enter VR Mode */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static bool EnterVRMode();

	/** Exit VR Mode */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void ExitVRMode();

	/** Whether location grid snapping is enabled */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static bool IsLocationGridSnappingEnabled();

	/** Toggle location grid snapping */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void ToggleLocationGridSnapping();

	/** Whether rotation grid snapping is enabled */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static bool IsRotationGridSnappingEnabled();

	/** Toggle rotation grid snapping */
	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static void ToggleRotationGridSnapping();

private:

	FDelegateHandle EngineInitCompleteDelegate;

	void OnEngineInitComplete();
	
	// Static IDs when submitting open/close requests for the VProd main menu panels. VREditorUISystem uses FNames to manage its panels, so these should be used for consistency.	
	static const FName VProdPanelID;
	static const FName VProdPanelLeftID;
	static const FName VProdPanelRightID;
	static const FName VProdPanelContextID;
	static const FName VProdPanelTimelineID;
	static const FName VProdPanelMeasureID;
	static const FName VProdPanelGafferID;
};
