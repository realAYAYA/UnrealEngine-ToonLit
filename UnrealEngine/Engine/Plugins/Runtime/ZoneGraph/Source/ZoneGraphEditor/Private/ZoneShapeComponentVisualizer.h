// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "HitProxies.h"
#include "ComponentVisualizer.h"
#include "ZoneShapeComponent.h"
#include "ZoneShapeComponentVisualizer.generated.h"

class AActor;
class FEditorViewportClient;
class FMenuBuilder;
class FPrimitiveDrawInterface;
class FSceneView;
class FUICommandList;
class FViewport;
class SWidget;
struct FViewportClick;
struct FConvexVolume;

UENUM()
enum class FZoneShapeControlPointType : uint8
{
	None,
	In,
	Out,
};

/** Selection state data that will be captured by scoped transactions.*/
UCLASS(Transient)
class ZONEGRAPHEDITOR_API UZoneShapeComponentVisualizerSelectionState : public UObject
{
	GENERATED_BODY()
public:
	FComponentPropertyPath GetShapePropertyPath() const { return ShapePropertyPath; }
	void SetShapePropertyPath(const FComponentPropertyPath& InShapePropertyPath) { ShapePropertyPath = InShapePropertyPath; }
	const TSet<int32>& GetSelectedPoints() const { return SelectedPoints; }
	TSet<int32>& ModifySelectedPoints() { return SelectedPoints; }
	int32 GetLastPointIndexSelected() const { return LastPointIndexSelected; }
	void SetLastPointIndexSelected(const int32 InLastPointIndexSelected) { LastPointIndexSelected = InLastPointIndexSelected; }
	int32 GetSelectedSegmentIndex() const { return SelectedSegmentIndex; }
	void SetSelectedSegmentIndex(const int32 InSelectedSegmentIndex) { SelectedSegmentIndex = InSelectedSegmentIndex; }
	FVector GetSelectedSegmentPoint() const { return SelectedSegmentPoint; }
	void SetSelectedSegmentPoint(const FVector& InSelectedSegmentPoint) { SelectedSegmentPoint = InSelectedSegmentPoint; }
	float GetSelectedSegmentT() const { return SelectedSegmentT; }
	void SetSelectedSegmentT(const float InSelectedSegmentT) { SelectedSegmentT = InSelectedSegmentT; }
	int32 GetSelectedControlPoint() const { return SelectedControlPoint; }
	void SetSelectedControlPoint(const int32 InSelectedControlPoint) { SelectedControlPoint = InSelectedControlPoint; }
	FZoneShapeControlPointType GetSelectedControlPointType() const { return SelectedControlPointType; }
	void SetSelectedControlPointType(const FZoneShapeControlPointType InSelectedControlPointType) { SelectedControlPointType = InSelectedControlPointType; }

protected:
	/** Property path from the parent actor to the component */
	UPROPERTY()
	FComponentPropertyPath ShapePropertyPath;
	/** Index of keys we have selected */
	UPROPERTY()
	TSet<int32> SelectedPoints;
	/** Index of the last key we selected */
	UPROPERTY()
	int32 LastPointIndexSelected = 0;
	/** Index of segment we have selected */
	UPROPERTY()
	int32 SelectedSegmentIndex = 0;
	/** Position on selected segment */
	UPROPERTY()
	FVector SelectedSegmentPoint = FVector::ZeroVector;
	/** Interpolation value along the selected segment */
	UPROPERTY()
	float SelectedSegmentT = 0.0f;
	/** Index of tangent handle we have selected */
	UPROPERTY()
	int32 SelectedControlPoint = 0;
	/** The type of the selected tangent handle */
	UPROPERTY()
	FZoneShapeControlPointType SelectedControlPointType = FZoneShapeControlPointType::None;
};

/** Base class for clickable shape editing proxies */
struct ZONEGRAPHEDITOR_API HZoneShapeVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HZoneShapeVisProxy(const UActorComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HComponentVisProxy(InComponent, InPriority)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** Proxy for a shape point */
struct ZONEGRAPHEDITOR_API HZoneShapePointProxy : public HZoneShapeVisProxy
{
	DECLARE_HIT_PROXY();

	HZoneShapePointProxy(const UActorComponent* InComponent, int32 InPointIndex)
		: HZoneShapeVisProxy(InComponent, HPP_Foreground)
		, PointIndex(InPointIndex)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int32 PointIndex;
};

/** Proxy for a shape segment */
struct ZONEGRAPHEDITOR_API HZoneShapeSegmentProxy : public HZoneShapeVisProxy
{
	DECLARE_HIT_PROXY();

	HZoneShapeSegmentProxy(const UActorComponent* InComponent, int32 InSegmentIndex)
		: HZoneShapeVisProxy(InComponent)
		, SegmentIndex(InSegmentIndex)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int32 SegmentIndex;
};

/** Proxy for a control point handle */
struct ZONEGRAPHEDITOR_API HZoneShapeControlPointProxy : public HZoneShapeVisProxy
{
	DECLARE_HIT_PROXY();

	HZoneShapeControlPointProxy(const UActorComponent* InComponent, int32 InPointIndex, bool bInInControlPoint)
		: HZoneShapeVisProxy(InComponent, HPP_Foreground)
		, PointIndex(InPointIndex)
		, bInControlPoint(bInInControlPoint)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int32 PointIndex;
	bool bInControlPoint;
};

/** ZoneShapeComponent visualizer/edit functionality */
class ZONEGRAPHEDITOR_API FZoneShapeComponentVisualizer : public FComponentVisualizer, public FGCObject
{
public:
	FZoneShapeComponentVisualizer();
	virtual ~FZoneShapeComponentVisualizer();

	//~ Begin FComponentVisualizer Interface
	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	/** Handle box select input */
	virtual bool HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	/** Handle frustum select input */
	virtual bool HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	/** Return whether focus on selection should focus on bounding box defined by active visualizer */
	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;
	/** Pass snap input to active visualizer */
	virtual bool HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination) override;
	/** Get currently edited component, this is needed to reset the active visualizer after undo/redo */
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	virtual bool IsVisualizingArchetype() const override;
	//~ End FComponentVisualizer Interface

	/** Get the shape component we are currently editing */
	UZoneShapeComponent* GetEditedShapeComponent() const;

	const TSet<int32>& GetSelectedPoints() const
	{
		check(SelectionState);
		return SelectionState->GetSelectedPoints();
	}

protected:

	/** Determine if any selected key index is out of range (perhaps because something external has modified the shape) */
	bool IsAnySelectedPointIndexOutOfRange(const UZoneShapeComponent& Comp) const;

	/** Whether a single point is currently selected */
	bool IsSinglePointSelected() const;

	/** Transforms selected control point by given translation */
	bool TransformSelectedControlPoint(const FVector& DeltaTranslate);

	/** Transforms selected points by given delta translation, rotation and scale. */
	bool TransformSelectedPoints(const FEditorViewportClient* ViewportClient, const FVector& DeltaTranslate, const FRotator& DeltaRotate, const FVector& DeltaScale) const;

	/** Update the key selection state of the visualizer */
	void ChangeSelectionState(int32 Index, bool bIsCtrlHeld) const;

	/** Alt-drag: duplicates the selected point */
	bool DuplicatePointForAltDrag(const FVector& InDrag) const;

	/** Split segment using given interpolation value, selects the new point */
	void SplitSegment(const int32 InSegmentIndex, const float SegmentSplitT) const;

	/** Duplicates selected points and selects them. */
	void DuplicateSelectedPoints(const FVector& WorldOffset = FVector::ZeroVector, bool bInsertAfter = true) const;

	/** Updates the component and selected properties if the component has changed */
	const UZoneShapeComponent* UpdateSelectedShapeComponent(const HComponentVisProxy* VisProxy);

	/** Returns the rotation of last selected point, or false if last selection is not valid. */
	bool GetLastSelectedPointRotation(FQuat& OutRotation) const;

	void OnDeletePoint() const;
	bool CanDeletePoint() const;

	/** Duplicates selected points in place */
	void OnDuplicatePoint() const;
	bool IsPointSelectionValid() const;

	void OnAddPointToSegment() const;
	bool CanAddPointToSegment() const;

	void OnSetPointType(FZoneShapePointType Type) const;
	bool IsPointTypeSet(FZoneShapePointType Type) const;

	void OnSelectAllPoints() const;
	bool CanSelectAllPoints() const;

	void GenerateShapePointTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FZoneShapeComponentVisualizer");
	}
	// End of FGCObject interface

	/** Output log commands */
	TSharedPtr<FUICommandList> ShapeComponentVisualizerActions;

	/** Property to be notified when points change */
	FProperty* ShapePointsProperty;

	/** Current selection state */
	UZoneShapeComponentVisualizerSelectionState* SelectionState;

	/** Whether we currently allow duplication when dragging */
	bool bAllowDuplication;

	/** Alt-drag: Accumulates delayed drag offset. */
	FVector DuplicateAccumulatedDrag;

	/** True if the ControlPointPosition has been initialized */
	bool bControlPointPositionCaptured;

	/** Selected control pints adjusted position, allows the gizmo to move freely, while we constrain the control point. */
	FVector ControlPointPosition;

	/** True if cached rotation is set. */
	bool bHasCachedRotation = false;

	/** Rotation cached when mouse button is pressed. */
	FQuat CachedRotation = FQuat::Identity;

	bool bIsSelectingComponent = false;
};
