// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"
#include "Components/SplineComponent.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GenericPlatform/ICursor.h"
#include "HitProxies.h"
#include "InputCoreTypes.h"
#include "Math/Axis.h"
#include "Math/Box.h"
#include "Math/InterpCurvePoint.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "SplineComponentVisualizer.generated.h"

class AActor;
class FCanvas;
class FEditorViewportClient;
class FMenuBuilder;
class FPrimitiveDrawInterface;
class FProperty;
class FSceneView;
class FUICommandList;
class FViewport;
class SSplineGeneratorPanel;
class SWidget;
class SWindow;
class UActorComponent;
class USplineComponent;
class USplineMetadata;
struct FConvexVolume;
struct FViewportClick;

/** Tangent handle selection modes. */
UENUM()
enum class ESelectedTangentHandle
{
	None,
	Leave,
	Arrive
};

/** Selection state data that will be captured by scoped transactions.*/
UCLASS(Transient)
class COMPONENTVISUALIZERS_API USplineComponentVisualizerSelectionState : public UObject
{
	GENERATED_BODY()

public:

	/** Checks LastKeyIndexSelected is valid given the number of splint points and returns its value. */
	int32 GetVerifiedLastKeyIndexSelected(const int32 InNumSplinePoints) const;

	/** Checks TangentHandle and TangentHandleType are valid and sets relevant output parameters. */
	void GetVerifiedSelectedTangentHandle(const int32 InNumSplinePoints, int32& OutSelectedTangentHandle, ESelectedTangentHandle& OutSelectedTangentHandleType) const;

	const FComponentPropertyPath GetSplinePropertyPath() const { return SplinePropertyPath; }
	void SetSplinePropertyPath(const FComponentPropertyPath& InSplinePropertyPath) { SplinePropertyPath = InSplinePropertyPath; }

	const TSet<int32>& GetSelectedKeys() const { return SelectedKeys; }
	TSet<int32>& ModifySelectedKeys() { return SelectedKeys; }

	int32 GetLastKeyIndexSelected() const { return LastKeyIndexSelected; }
	void SetLastKeyIndexSelected(const int32 InLastKeyIndexSelected) { LastKeyIndexSelected = InLastKeyIndexSelected; }

	int32 GetSelectedSegmentIndex() const { return SelectedSegmentIndex; }
	void SetSelectedSegmentIndex(const int32 InSelectedSegmentIndex) { SelectedSegmentIndex = InSelectedSegmentIndex; }

	int32 GetSelectedTangentHandle() const { return SelectedTangentHandle; }
	void SetSelectedTangentHandle(const int32 InSelectedTangentHandle) { SelectedTangentHandle = InSelectedTangentHandle; }

	ESelectedTangentHandle GetSelectedTangentHandleType() const { return SelectedTangentHandleType; }
	void SetSelectedTangentHandleType(const ESelectedTangentHandle InSelectedTangentHandle) { SelectedTangentHandleType = InSelectedTangentHandle; }

	FVector GetSelectedSplinePosition() const { return SelectedSplinePosition; }
	void SetSelectedSplinePosition(const FVector& InSelectedSplinePosition) { SelectedSplinePosition = InSelectedSplinePosition; }

	FQuat GetCachedRotation() const { return CachedRotation; }
	void SetCachedRotation(const FQuat& InCachedRotation) { CachedRotation = InCachedRotation; }

	void Reset();
	void ClearSelectedSegmentIndex();
	void ClearSelectedTangentHandle();

	bool IsSplinePointSelected(const int32 InIndex) const;

protected:
	/** Property path from the parent actor to the component */
	UPROPERTY()
	FComponentPropertyPath SplinePropertyPath;

	/** Indices of keys we have selected */
	UPROPERTY()
	TSet<int32> SelectedKeys;

	/** Index of the last key we selected */
	UPROPERTY()
	int32 LastKeyIndexSelected = INDEX_NONE;

	/** Index of segment we have selected */
	UPROPERTY()
	int32 SelectedSegmentIndex = INDEX_NONE;

	/** Index of tangent handle we have selected */
	UPROPERTY()
	int32 SelectedTangentHandle = INDEX_NONE;

	/** The type of the selected tangent handle */
	UPROPERTY()
	ESelectedTangentHandle SelectedTangentHandleType = ESelectedTangentHandle::None;

	/** Position on spline we have selected */
	UPROPERTY()
	FVector SelectedSplinePosition;

	/** Cached rotation for this point */
	UPROPERTY()
	FQuat CachedRotation;
};

/** Base class for clickable spline editing proxies */
struct HSplineVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineVisProxy(const UActorComponent* InComponent)
	: HComponentVisProxy(InComponent, HPP_Wireframe)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** Proxy for a spline key */
struct HSplineKeyProxy : public HSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineKeyProxy(const UActorComponent* InComponent, int32 InKeyIndex) 
		: HSplineVisProxy(InComponent)
		, KeyIndex(InKeyIndex)
	{}

	int32 KeyIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** Proxy for a spline segment */
struct HSplineSegmentProxy : public HSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineSegmentProxy(const UActorComponent* InComponent, int32 InSegmentIndex)
		: HSplineVisProxy(InComponent)
		, SegmentIndex(InSegmentIndex)
	{}

	int32 SegmentIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** Proxy for a tangent handle */
struct HSplineTangentHandleProxy : public HSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineTangentHandleProxy(const UActorComponent* InComponent, int32 InKeyIndex, bool bInArriveTangent)
		: HSplineVisProxy(InComponent)
		, KeyIndex(InKeyIndex)
		, bArriveTangent(bInArriveTangent)
	{}

	int32 KeyIndex;
	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** Accepted modes for snapping points. */
enum class ESplineComponentSnapMode
{
	Snap,
	AlignToTangent,
	AlignPerpendicularToTangent
};

/** SplineComponent visualizer/edit functionality */
class COMPONENTVISUALIZERS_API FSplineComponentVisualizer : public FComponentVisualizer, public FGCObject
{
public:
	FSplineComponentVisualizer();
	virtual ~FSplineComponentVisualizer();

	//~ Begin FComponentVisualizer Interface
	virtual void OnRegister() override;
	virtual bool ShouldShowForSelectedSubcomponents(const UActorComponent* Component) override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	/** Draw HUD on viewport for the supplied component */
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	/** Handle click modified by Alt, Ctrl and/or Shift. The input HitProxy may not be on this component. */
	virtual bool HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	/** Handle box select input */
	virtual bool HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	/** Handle frustum select input */
	virtual bool HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	/** Return whether focus on selection should focus on bounding box defined by active visualizer */
	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;
	/** Pass snap input to active visualizer */
	virtual bool HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination) override;
	/** Gets called when the mouse tracking has stopped (dragging behavior) */
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	/** Get currently edited component, this is needed to reset the active visualizer after undo/redo */
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	virtual bool IsVisualizingArchetype() const override;
	//~ End FComponentVisualizer Interface

	/** Add menu sections to the context menu */
	virtual void GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const;

	/** Get the spline component we are currently editing */
	USplineComponent* GetEditedSplineComponent() const;

	const TSet<int32>& GetSelectedKeys() const { check(SelectionState); return SelectionState->GetSelectedKeys(); }

	/** Select first or last spline point, returns true if the spline component being edited has changed */
	bool HandleSelectFirstLastSplinePoint(USplineComponent* InSplineComponent, bool bFirstPoint);

	/** Select all spline points, , returns true if the spline component being edited has changed */
	bool HandleSelectAllSplinePoints(USplineComponent* InSplineComponent);

	/** Select next or prev spline point, loops when last point is currently selected */
	void OnSelectPrevNextSplinePoint(bool bNextPoint, bool bAddToSelection);

	/** Sets the new cached rotation on the visualizer */
	void SetCachedRotation(const FQuat& NewRotation);
protected:

	/** Determine if any selected key index is out of range (perhaps because something external has modified the spline) */
	bool IsAnySelectedKeyIndexOutOfRange(const USplineComponent* Comp) const;

	/** Whether a single spline key is currently selected */
	bool IsSingleKeySelected() const;
	
	/** Whether a multiple spline keys are currently selected */
	bool AreMultipleKeysSelected() const;

	/** Whether any keys are currently selected */
	bool AreKeysSelected() const;

	/** Select spline point at specified index */
	void SelectSplinePoint(int32 SelectIndex, bool bAddToSelection);

	/** Transforms selected tangent by given translation */
	bool TransformSelectedTangent(EPropertyChangeType::Type InPropertyChangeType, const FVector& InDeltaTranslate);

	/** Transforms selected tangent by given translate, rotate and scale */
	bool TransformSelectedKeys(EPropertyChangeType::Type InPropertyChangeType, const FVector& InDeltaTranslate, const FRotator& InDeltaRotate = FRotator::ZeroRotator, const FVector& InDeltaScale = FVector::ZeroVector);

	/** Update the key selection state of the visualizer */
	virtual void ChangeSelectionState(int32 Index, bool bIsCtrlHeld);

	/** Alt-drag: duplicates the selected spline key */
	virtual bool DuplicateKeyForAltDrag(const FVector& InDrag);

	/** Alt-drag: updates duplicated selected spline key */
	virtual bool UpdateDuplicateKeyForAltDrag(const FVector& InDrag);

	/** Return spline data for point on spline closest to input point */
	virtual float FindNearest(const FVector& InLocalPos, int32 InSegmentStartIndex, FVector& OutSplinePos, FVector& OutSplineTangent) const;

	/** Split segment using given world position */
	virtual void SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex, bool bCopyFromSegmentBeginIndex = true);

	/** Update split segment based on drag offset */
	virtual void UpdateSplitSegment(const FVector& InDrag);

	/** Add segment to beginning or end of spline */
	virtual void AddSegment(const FVector& InWorldPos, bool bAppend);

	/** Add segment to beginning or end of spline */
	virtual void UpdateAddSegment(const FVector& InWorldPos);

	/** Alt-drag: duplicates the selected spline key */
	virtual void ResetAllowDuplication();

	/** Snapping: snap keys to axis position of last selected key */
	virtual void SnapKeysToLastSelectedAxisPosition(const EAxis::Type InAxis, TArray<int32> InSnapKeys);

	/** Snapping: snap key to selected actor */
	virtual void SnapKeyToActor(const AActor* InActor, const ESplineComponentSnapMode SnapMode);

	/** Snapping: generic method for snapping selected keys to given transform */
	virtual void SnapKeyToTransform(const ESplineComponentSnapMode InSnapMode,
		const FVector& InWorldPos,
		const FVector& InWorldUpVector,
		const FVector& InWorldForwardVector,
		const FVector& InScale,
		const USplineMetadata* InCopySplineMetadata = nullptr,
		const int32 InCopySplineMetadataKey = 0);

	/** Snapping: set snap to actor temporary mode */
	virtual void SetSnapToActorMode(const bool bInIsSnappingToActor, const ESplineComponentSnapMode InSnapMode = ESplineComponentSnapMode::Snap);

	/** Snapping: get snap to actor temporary mode */
	virtual bool GetSnapToActorMode(ESplineComponentSnapMode& OutSnapMode) const;

	/** Reset temporary modes after inputs are handled. */
	virtual void ResetTempModes();

	/** Updates the component and selected properties if the component has changed */
	const USplineComponent* UpdateSelectedSplineComponent(HComponentVisProxy* VisProxy);

	void OnDeleteKey();
	bool CanDeleteKey() const;

	/** Duplicates selected spline keys in place */
	void OnDuplicateKey();
	bool IsKeySelectionValid() const;

	void OnAddKeyToSegment();
	bool CanAddKeyToSegment() const;

	void OnSnapKeyToNearestSplinePoint(ESplineComponentSnapMode InSnapMode);

	void OnSnapKeyToActor(const ESplineComponentSnapMode InSnapMode);

	void OnSnapAllToAxis(EAxis::Type InAxis);

	void OnSnapSelectedToAxis(EAxis::Type InAxis);

	void OnStraightenKey(int32 Direction);
	void StraightenKey(int32 KeyToStraighten, int32 KeyToStraightenToward);

	void OnToggleSnapTangentAdjustment();
	bool IsSnapTangentAdjustment() const;

	void OnLockAxis(EAxis::Type InAxis);
	bool IsLockAxisSet(EAxis::Type InAxis) const; 

	void OnResetToAutomaticTangent(EInterpCurveMode Mode);
	bool CanResetToAutomaticTangent(EInterpCurveMode Mode) const;

	void OnSetKeyType(EInterpCurveMode Mode);
	bool IsKeyTypeSet(EInterpCurveMode Mode) const;

	void OnSetVisualizeRollAndScale();
	bool IsVisualizingRollAndScale() const;

	void OnSetDiscontinuousSpline();
	bool IsDiscontinuousSpline() const;

	void OnToggleClosedLoop();
	bool IsClosedLoop() const;

	void OnResetToDefault();
	bool CanResetToDefault() const;

	/** Select first or last spline point */
	void OnSelectFirstLastSplinePoint(bool bFirstPoint);

	/** Select all spline points, if no spline points selected yet the currently edited spline component will be set as well */
	void OnSelectAllSplinePoints();

	bool CanSelectSplinePoints() const;

	/** Generate the submenu containing available selection actions */
	void GenerateSelectSplinePointsSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available point types */
	void GenerateSplinePointTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available auto tangent types */
	void GenerateTangentTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available snap/align actions */
	void GenerateSnapAlignSubMenu(FMenuBuilder& MenuBuilder) const;
	
	/** Generate the submenu containing the lock axis types */
	void GenerateLockAxisSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Helper function to set edited component we are currently editing */
	void SetEditedSplineComponent(const USplineComponent* InSplineComponent);

	void CreateSplineGeneratorPanel();
	
	void OnDeselectedInEditor(TObjectPtr<USplineComponent> SplineComponent);

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSplineComponentVisualizer");
	}
	// End of FGCObject interface

	/** Output log commands */
	TSharedPtr<FUICommandList> SplineComponentVisualizerActions;

	/** Current selection state */
	TObjectPtr<USplineComponentVisualizerSelectionState> SelectionState;

	/** Whether we currently allow duplication when dragging */
	bool bAllowDuplication;

	/** Alt-drag: True when in process of duplicating a spline key. */
	bool bDuplicatingSplineKey;

	/** Alt-drag: True when in process of adding end segment. */
	bool bUpdatingAddSegment;

	/** Alt-drag: Delays duplicating control point to accumulate sufficient drag input offset. */
	uint32 DuplicateDelay;

	/** Alt-drag: Accumulates delayed drag offset. */
	FVector DuplicateDelayAccumulatedDrag;

	/** Alt-drag: Cached segment parameter for split segment at new control point */
	float DuplicateCacheSplitSegmentParam;

	/** Axis to fix when adding new spline points. Uses the value of the currently 
	    selected spline point's X, Y, or Z value when fix is not equal to none. */
	EAxis::Type AddKeyLockedAxis;

	/** Snap: True when in process of snapping to actor which needs to be Ctrl-Selected. */
	bool bIsSnappingToActor;

	/** Snap: Snap to actor mode. */
	ESplineComponentSnapMode SnapToActorMode;

	FProperty* SplineCurvesProperty;

	FDelegateHandle DeselectedInEditorDelegateHandle;

private:
	TSharedPtr<SSplineGeneratorPanel> SplineGeneratorPanel;
	static TWeakPtr<SWindow> WeakExistingWindow;
};
