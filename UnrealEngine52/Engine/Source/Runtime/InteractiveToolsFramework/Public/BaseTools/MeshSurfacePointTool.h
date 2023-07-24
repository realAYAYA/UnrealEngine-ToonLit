// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SingleSelectionTool.h"
#include "InteractiveToolBuilder.h"
#include "InputBehaviorSet.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BaseBehaviors/AnyButtonInputBehavior.h"
#include "MeshSurfacePointTool.generated.h"



class UMeshSurfacePointTool;

// This is a temporary interface to provide stylus pressure, currently necessary due to limitations
// in the stylus plugin architecture. Should be removed once InputState/InputBehavior can support stylus events
class INTERACTIVETOOLSFRAMEWORK_API IToolStylusStateProviderAPI
{
public:
	virtual float GetCurrentPressure() const = 0;
};

/**
 * 
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UMeshSurfacePointToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:
	IToolStylusStateProviderAPI* StylusAPI = nullptr;

	/** @return true if a single mesh source can be found in the active selection */
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	/** @return new Tool instance initialized with selected mesh source */
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	/** @return new Tool instance. Override this in subclasses to build a different Tool class type */
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const;

	/** Called by BuildTool to configure the Tool with the input MeshSource based on the SceneState */
	virtual void InitializeNewTool(UMeshSurfacePointTool* Tool, const FToolBuilderState& SceneState) const;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};



/**
 * UMeshSurfacePointTool is a base Tool implementation that can be used to implement various
 * "point on surface" interactions. The tool acts on an input IMeshDescriptionSource object,
 * which the standard Builder can extract from the current selection (eg Editor selection).
 * 
 * Subclasses override the OnBeginDrag/OnUpdateDrag/OnEndDrag and OnUpdateHover functions
 * to implement custom behavior.
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UMeshSurfacePointTool : public USingleSelectionTool, public IClickDragBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	// UInteractiveTool API Implementation

	/** Register InputBehaviors, etc */
	virtual void Setup() override;

	/** Set current stlyus API source */
	virtual void SetStylusAPI(IToolStylusStateProviderAPI* StylusAPI);

	// UMeshSurfacePointTool API

	/**
	 * @return true if the target MeshSource is hit by the Ray
	 */
	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit);

	/**
	 * This function is called when the user begins a click-drag-release interaction
	 */
	virtual void OnBeginDrag(const FRay& Ray);

	/**
	 * This function is called each frame that the user is in a click-drag-release interaction
	 */
	virtual void OnUpdateDrag(const FRay& Ray);

	/**
	 * This function is called when the user releases the button driving a click-drag-release interaction
	 */
	virtual void OnEndDrag(const FRay& Ray);




	// IClickDragBehaviorTarget implementation
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;


	// IHoverBehaviorTarget implementation
	
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override { return true; }
	virtual void OnEndHover() override {}


	// IInteractiveToolCameraFocusAPI implementation
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;


	/** Called by registered InputBehaviors to set the state of the "shift" button (or device equivalent) */
	virtual void SetShiftToggle(bool bShiftDown);

	/** @return current state of the shift toggle */
	virtual bool GetShiftToggle() const { return bShiftToggle; }

	/** Called by registered InputBehaviors to set the state of the "shift" button (or device equivalent) */
	virtual void SetCtrlToggle(bool bCtrlDown);

	/** @return current state of the shift toggle */
	virtual bool GetCtrlToggle() const { return bCtrlToggle; }

	/** @return current input device pressure in range 0-1 */
	virtual float GetCurrentDevicePressure() const;

	void SetWorld(UWorld* World);
	UWorld* GetTargetWorld();

protected:
	/** Current state of the shift modifier toggle */
	bool bShiftToggle = false;

	/** Current state of the ctrl modifier toggle */
	bool bCtrlToggle = false;

	FRay LastWorldRay;

	IToolStylusStateProviderAPI* StylusAPI = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;
};

