// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "MultiSelectionTool.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "BaseGizmos/TransformProxy.h"
#include "BaseSequencerAnimTool.h"
#include "ControlRig.h"
#include "Styling/AppStyle.h"
#include "SequencerAnimEditPivotTool.generated.h"

class USingleClickInputBehavior;
class UClickDragInputBehavior;
class UCombinedTransformGizmo;
class ULevelSequence;
struct FRigControlElement;
class ISequencer;
class FUICommandList;
class IAssetViewport;
/*
*  The way this sequencer pivot tool works is that
*  it will use modify the incoming selections temp pivot
*  while the mode is active. Reselecting will turn off the mode.
*
*/

/**
 * Builder for USequencerPivotTool
 */
UCLASS()
class  USequencerPivotToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

struct FSelectionDuringDrag
{
	ULevelSequence* LevelSequence;
	FFrameNumber CurrentFrame;
	FTransform CurrentTransform;

};
struct FControlRigSelectionDuringDrag : public FSelectionDuringDrag
{
	UControlRig* ControlRig;
	FName ControlName;
};

struct FActorSelectonDuringDrag : public FSelectionDuringDrag
{
	AActor* Actor;
};

struct FControlRigMappings
{
	TWeakObjectPtr<UControlRig> ControlRig;

	FTransform GetParentTransform() const;
	TOptional<FTransform> GetWorldTransform(const FName& Name) const;
	void SetFromWorldTransform(const FName& Name, const FTransform& WorldTransform);
	TArray<FName> GetAllControls() const;
	void SelectControls();
	bool IsAnyControlDeselected() const;
private:
	TMap<FName, FTransform> PivotTransforms;
};

struct FActorMappings
{
	TWeakObjectPtr<AActor> Actor;

	TOptional<FTransform> GetWorldTransform() const;
	void SetFromWorldTransform(const FTransform& WorldTransform);
	void SelectActors();
private:
	FTransform PivotTransform;
};

struct FSavedMappings
{
	TMap<TWeakObjectPtr<UControlRig>, FControlRigMappings> ControlRigMappings;
	TMap<TWeakObjectPtr<AActor>, FActorMappings> ActorMappings;
};

struct FLastSelectedObjects
{
	TArray<FControlRigMappings> LastSelectedControlRigs;
	TArray<FActorMappings> LastSelectedActors;

};
class FEditPivotCommands : public TCommands<FEditPivotCommands>
{
public:
	FEditPivotCommands()
		: TCommands<FEditPivotCommands>(
			"SequencerEditPivotTool",
			NSLOCTEXT("SequencerEditPivotTool", "SequencerEditPivotTool", "Edit Pivot Commands"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
			)
	{}


	virtual void RegisterCommands() override;
	static const TMap<FName, TArray<TSharedPtr<FUICommandInfo>>>& GetCommands()
	{
		return FEditPivotCommands::Get().Commands;
	}

public:
	/** Reset the Pivot*/
	TSharedPtr<FUICommandInfo> ResetPivot;
	/** Toggle Free/Pivot Mode*/
	TSharedPtr<FUICommandInfo> ToggleFreePivot;

	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};


/**
Pivot tool class
 */
UCLASS()
class  USequencerPivotTool : public UMultiSelectionTool, public IClickBehaviorTarget  , public IBaseSequencerAnimTool
{
	GENERATED_BODY()

public:

	// UInteractiveTool overrides
	virtual void SetWorld(UWorld* World, UInteractiveGizmoManager* GizmoManager);
	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }
	virtual void OnTick(float DeltaTime) override;

	// IClickBehaviorTarget interface
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	//IBaseSequencerAnimTool
	virtual bool ProcessCommandBindings(const FKey Key, const bool bRepeat) const override;

	// End interfaces

	//If In Pivot Mode, if not then in Free Mode
	bool IsInPivotMode() const { return bInPivotMode; }
	//Toggle Pivot Mode
	void TogglePivotMode() { SetPivotMode(!bInPivotMode); }
	//Set Pivot Mode
	void SetPivotMode(bool bVal);
protected:

	UPROPERTY()
	TObjectPtr<USingleClickInputBehavior> ClickBehavior = nullptr;

	UPROPERTY()
	TObjectPtr<UTransformProxy> TransformProxy = nullptr;

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> TransformGizmo = nullptr;

protected:
	bool bInPivotMode = false; //pivot may be in 'free' mode or 'pivot' mode
	bool bShiftPressedWhenStarted = false;
	bool bCtrlPressedWhenStarted = false;
	UWorld* TargetWorld = nullptr;		// target World we will raycast into
	UInteractiveGizmoManager* GizmoManager = nullptr;

	FTransform StartDragTransform;
	bool bGizmoBeingDragged = false;
	bool bManipulatorMadeChange = false;
	int32 TransactionIndex = -1;
	TArray<FControlRigSelectionDuringDrag> ControlRigDrags;
	TArray<FActorSelectonDuringDrag> ActorDrags;
	TArray<TSharedPtr<FControlRigInteractionScope>> InteractionScopes;

	//since we are selection based we can cache this
	ULevelSequence* LevelSequence;
	TArray<TWeakObjectPtr<UControlRig>> ControlRigs;
	TWeakPtr<ISequencer> SequencerPtr;
	TArray<TWeakObjectPtr<AActor>> Actors;

	FTransform GizmoTransform = FTransform::Identity;
	bool bPickingPivotLocation = false; //mauy remove
	void UpdateGizmoVisibility();
	void UpdateGizmoTransform();
	void UpdateTransformAndSelectionOnEntering();
	bool SetGizmoBasedOnSelection(bool bUseSaved = true);

	FInputRayHit FindRayHit(const FRay& WorldRay, FVector& HitPos);		// raycasts into World

	// Callbacks we'll receive from the gizmo proxy
	void GizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform);
	void GizmoTransformStarted(UTransformProxy* Proxy);
	void GizmoTransformEnded(UTransformProxy* Proxy);

	//Handle Selection and Pivot Location
	void SavePivotTransforms();
	void SaveLastSelected();

	// selection delegates
	void DeactivateMe();
	void RemoveDelegates();
	void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected);
	void OnEditorSelectionChanged(UObject* NewSelection);
	FDelegateHandle OnEditorSelectionChangedHandle;

	// Functions and variables for handling the overlay for switching between free and pivot mode
	void CreateAndShowPivotOverlay();
	void RemoveAndDestroyPivotOverlay();

	FMargin GetPivotOverlayPadding() const;
	
	static FVector2D LastPivotOverlayLocation;
	TSharedPtr<SWidget> PivotWidget;
public:
	void TryRemovePivotOverlay();
	void TryShowPivotOverlay();
	void UpdatePivotOverlayLocation(const FVector2D InLocation, TSharedPtr<IAssetViewport> ActiveLevelViewport);

private:
	TSharedPtr<FUICommandList> CommandBindings;
	void ResetPivot();

public:
	static FSavedMappings SavedPivotLocations;
	static FLastSelectedObjects LastSelectedObjects;
};


