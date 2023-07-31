// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Drawing/PreviewGeometryActor.h"
#include "Drawing/TriangleSetComponent.h"
#include "InputBehavior.h"
#include "InteractiveTool.h"
#include "Mechanics/RectangleMarqueeMechanic.h"
#include "Components/DynamicMeshComponent.h"
#include "Selection/GroupTopologySelector.h"
#include "TransformTypes.h"
#include "ToolDataVisualizer.h"
#include "InteractionMechanic.h"
#include "PolygonSelectionMechanic.generated.h"

class FPolygonSelectionMechanicSelectionChange;
class UPersistentMeshSelection;
class UMouseHoverBehavior;
class UPolygonSelectionMechanic;
class URectangleMarqueeMechanic;
class USingleClickOrDragInputBehavior;

using UE::Geometry::FDynamicMeshAABBTree3;
PREDECLARE_USE_GEOMETRY_CLASS(FCompactMaps);

UCLASS()
class MODELINGCOMPONENTS_API UPolygonSelectionMechanicProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = SelectionFilter)
	bool bSelectVertices = true;

	UPROPERTY(EditAnywhere, Category = SelectionFilter)
	bool bSelectEdges = true;

	UPROPERTY(EditAnywhere, Category = SelectionFilter)
	bool bSelectFaces = true;

	/** When set, will select edge loops. Edge loops are paths along a string of valence-4 vertices. */
	UPROPERTY(EditAnywhere, Category = SelectionFilter)
	bool bSelectEdgeLoops = false;

	/** When set, will select rings of edges that are opposite each other across a quad face. */
	UPROPERTY(EditAnywhere, Category = SelectionFilter)
	bool bSelectEdgeRings = false;

	/** When false, faces that face away from the camera are ignored in selection and occlusion. Useful for working with inside-out meshes. */
	UPROPERTY(EditAnywhere, Category = AdditionalSelectionOptions, AdvancedDisplay)
	bool bHitBackFaces = true;

	UPROPERTY(EditAnywhere, Category = AdditionalSelectionOptions, AdvancedDisplay)
	bool bEnableMarquee = true;

	/** Determines whether vertices should be checked for occlusion in marquee select (Note: marquee select currently only works with edges and vertices) */
	UPROPERTY(EditAnywhere, Category = AdditionalSelectionOptions, meta = (EditCondition = "bEnableMarquee", EditConditionHides))
	bool bMarqueeIgnoreOcclusion = true;

	// The following were originally in their own category, all marked as AdvancedDisplay. However, since there wasn't a non-AdvancedDisplay
	// property in the category, they started out as expanded and could not be collapsed.
	// The alternative approach, used below, is to have them in a nested category, which starts out as collapsed. This works nicely.

	/** Prefer to select an edge projected to a point rather than the point, or a face projected to an edge rather than the edge. */
	UPROPERTY(EditAnywhere, Category = "AdditionalSelectionOptions|Ortho Viewport Behavior")
	bool bPreferProjectedElement = true;

	/** If the closest element is valid, select other elements behind it that are aligned with it. */
	UPROPERTY(EditAnywhere, Category = "AdditionalSelectionOptions|Ortho Viewport Behavior")
	bool bSelectDownRay = true;

	/** Do not check whether the closest element is occluded from the current view. */
	UPROPERTY(EditAnywhere, Category = "AdditionalSelectionOptions|Ortho Viewport Behavior")
	bool bIgnoreOcclusion = false;

	// Used to avoid showing some of the selection filter buttons in triedit (in the detail customization)
	bool bDisplayPolygroupReliantControls = true;

	/** Invert current selection. If selection is empty, has same effect as Select All, and is similarly dependent on selection filter. */
	UFUNCTION(CallInEditor, Category = SelectionActions)
	void InvertSelection();

	/** Select all elements. Depends on selection filter, where vertices are preferred to edges to faces. */
	UFUNCTION(CallInEditor, Category = SelectionActions)
	void SelectAll();

	void Initialize(UPolygonSelectionMechanic* MechanicIn)
	{
		Mechanic = MechanicIn;
	}

protected:
	TWeakObjectPtr<UPolygonSelectionMechanic> Mechanic;
};



/**
 * UPolygonSelectionMechanic implements the interaction for selecting a set of faces/vertices/edges
 * from a FGroupTopology on a UDynamicMeshComponent. 
 */
UCLASS()
class MODELINGCOMPONENTS_API UPolygonSelectionMechanic : public UInteractionMechanic, public IClickBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()
	using FFrame3d = UE::Geometry::FFrame3d;
	using FAxisAlignedBox3d = UE::Geometry::FAxisAlignedBox3d;
public:

	virtual ~UPolygonSelectionMechanic();

	// configuration variables that must be set before bSetup is called
	bool bAddSelectionFilterPropertiesToParentTool = true;

	virtual void Setup(UInteractiveTool* ParentTool) override;
	virtual void Shutdown() override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI);

	/**
	 * Initializes the mechanic.
	 *
	 * @param Mesh Mesh that we are operating on.
	 * @param TargetTransform Transform of the mesh.
	 * @param World World in which we are operating, used to add drawing components that draw highlighted edges.
	 * @param Topology Group topology of the mesh.
	 * @param GetSpatialSourceFunc Function that returns an AABB tree for the mesh.
	 */
	void Initialize(const FDynamicMesh3* Mesh,
		FTransform3d TargetTransform,
		UWorld* World,
		const FGroupTopology* Topology,
		TFunction<FDynamicMeshAABBTree3*()> GetSpatialSourceFunc
		);

	void Initialize(UDynamicMeshComponent* MeshComponent, const FGroupTopology* Topology,
		TFunction<FDynamicMeshAABBTree3 * ()> GetSpatialSourceFunc
	);

	/**
	 * Removes the mechanic's own click/hover handlers, which means that the parent tool
	 * will need to call UpdateSelection(), UpdateHighlight(), ClearHighlight(), and 
	 * ClearSelection() from its own hover/click handlers.
	 *
	 * Must be called during tool Setup() after calling Setup() on the mechanic to have an effect.
	 *
	 * @param ParentToolIn The parent tool, needed to be able to remove the behaviors.
	 */
	void DisableBehaviors(UInteractiveTool* ParentToolIn);

	/**
	 * Enable/disable the mechanic without permanently removing behaviors or shutting it down.
	 */
	void SetIsEnabled(bool bOn);

	/**
	 * Sets the base priority so that tools can make sure that their own behaviors are higher
	 * priority. The mechanic will not use any priority value higher than this, but it may use
	 * lower if it needs to stagger the priorities of behaviors it uses.
	 * Can be called before or after Setup().
	 */
	void SetBasePriority(const FInputCapturePriority& Priority);

	/**
	 * Gets the current priority range used by behaviors in the mechanic. The returned pair will
	 * have the base (highest) priority as the key, and the lowest priority as the value.
	 */
	TPair<FInputCapturePriority, FInputCapturePriority> GetPriorityRange() const;


	void SetShouldSelectEdgeLoopsFunc(TFunction<bool(void)> Func)
	{
		ShouldSelectEdgeLoopsFunc = Func;
	}

	void SetShouldSelectEdgeRingsFunc(TFunction<bool(void)> Func)
	{
		ShouldSelectEdgeRingsFunc = Func;
	}

	/**
	 * By default, the shift key will cause new clicks to add to the selection. However, this
	 * can be changed by supplying a different function to check here.
	 */
	void SetShouldAddToSelectionFunc(TFunction<bool(void)> Func)
	{
		ShouldAddToSelectionFunc = Func;
	}

	/**
	 * By default, the Ctrl key will cause new clicks to remove from the existing selection.
	 * However, this can be changed by supplying a different function to check here.
	 */
	void SetShouldRemoveFromSelectionFunc(TFunction<bool(void)> Func)
	{
		ShouldRemoveFromSelectionFunc = Func;
	}

	/**
	 * Notify internal data structures that the associated MeshComponent has been modified.
	 * @param bTopologyModified if true, the underlying mesh topology has been changed. This clears the current selection.
	 */
	void NotifyMeshChanged(bool bTopologyModified);

	/**
	 * Perform a hit test on the topology using the current selection settings. In cases of hitting edges and
	 * corners, OutHit contains the following:
	 *   OutHit.FaceIndex: edge or corner id in the topology
	 *   OutHit.ImpactPoint: closest point on the ray to the hit element (Note: not a point on the element!)
	 *   OutHit.Distance: distance along the ray to ImpactPoint
	 *   OutHit.Item: if hit item was an edge, index of the segment within the edge polyline. Otherwise undefined.
	 *
	 * @param bUseOrthoSettings If true, the ortho-relevant settings for selection are used (selecting down the view ray, etc)
	 */
	bool TopologyHitTest(const FRay& WorldRay, FHitResult& OutHit, FGroupTopologySelection& OutSelection, bool bUseOrthoSettings = false);
	bool TopologyHitTest(const FRay& WorldRay, FHitResult& OutHit, bool bUseOrthoSettings = false);

	TSharedPtr<FGroupTopologySelector, ESPMode::ThreadSafe> GetTopologySelector() { return TopoSelector; }

	//
	// Hover API
	//

	/**
	 * Update the hover highlight based on the hit elements at the given World Ray
	 * @return true if something was hit and is now being hovered
	 */
	bool UpdateHighlight(const FRay& WorldRay);

	/**
	 * Clear current hover-highlight
	 */
	void ClearHighlight();


	//
	// Selection API
	//

	/** 
	 * Intersect the ray with the mesh and update the selection based on the hit element, modifier states, etc
	 * @return true if selection was modified
	 */
	bool UpdateSelection(const FRay& WorldRay, FVector3d& LocalHitPositionOut, FVector3d& LocalHitNormalOut);

	/**
	 * Replace the current selection with an external selection. 
	 * @warning does not check that the selection is valid!
	 *
	 * @param bBroadcast If true, issues an OnSelectionChanged delegate broadcast.
	 */
	void SetSelection(const FGroupTopologySelection& Selection, bool bBroadcast = true);

	/**
	 * Clear the current selection.
	 */
	void ClearSelection();

	void InvertSelection();
	void SelectAll();

	/** 
	 * @return true if the current selection is non-empty 
	 */
	bool HasSelection() const;

	/**
	 * @return the current selection
	 */
	const FGroupTopologySelection& GetActiveSelection() const { return PersistentSelection; }

	/**
	 * Gives the current selection as a storable selection object. Can optionally apply the passed-in
	 * compact maps to the object if the topology in the mechanic was not updated after compacting.
	 */
	void GetSelection(UPersistentMeshSelection& SelectionOut, const FCompactMaps* CompactMapsToApply = nullptr) const;

	/**
	 * Sets the current selection using the given storable selection object. The topology in the
	 * mechanic must already be initialized for this to work.
	 */
	void LoadSelection(const UPersistentMeshSelection& Selection);

	/**
	 * Can be used by in an OnSelectionChanged event to inspect the clicked location (i.e., the
	 * values returned by the UpdateSelection() function when the click happened).
	 */
	void GetClickedHitPosition(FVector3d& PositionOut, FVector3d& NormalOut) const;

	/**
	 * @return The best-guess 3D frame for the current select
	 * @param bWorld if true, local-to-world transform of the target MeshComponent is applied to the frame
	 */
	FFrame3d GetSelectionFrame(bool bWorld, FFrame3d* InitialLocalFrame = nullptr) const;

	/**
	 * @return Bounding box for the current selection
	 * @param bWorld if true, the box is in world space, otherwise it is in local space of the target MeshComponent
	 */
	FAxisAlignedBox3d GetSelectionBounds(bool bWorld) const;

	void SetShowSelectableCorners(bool bShowCorners);

	//
	// Change Tracking
	//

	/**
	 * Begin a change record. Internally creates a FCommandChange and initializes it with current state
	 */
	void BeginChange();

	/**
	 * End the active change and return it. Returns empty change if the selection was not modified!
	 */
	TUniquePtr<FToolCommandChange> EndChange();

	/**
	 * Ends the active change and emits it via the parent tool, if the selection has been modified.
	 */
	bool EndChangeAndEmitIfModified();

	// IClickBehaviorTarget implementation
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;

protected:
	// These get bound to marquee mechanic delegates.
	virtual void OnDragRectangleStarted();
	virtual void OnDragRectangleChanged(const FCameraRectangle& CurrentRectangle);
	virtual void OnDragRectangleFinished(const FCameraRectangle& Rectangle, bool bCancelled);

	virtual void UpdateMarqueeEnabled();

public:
	/** 
	 * OnSelectionChanged is broadcast whenever the selection is modified (including by FChanges, which
	 * means that called functions should not issue undo transactions. 
	 */
	FSimpleMulticastDelegate OnSelectionChanged;

	/**
	 * This is broadcast during marquee rectangle dragging if selected faces change, to allow user to
	 * dynamically update face highlighting if desired (needed because OnSelectionChanged is only 
	 * fired once the rectangle is completed, not while it is being updated).
	 */
	FSimpleMulticastDelegate OnFaceSelectionPreviewChanged;

	/**
	 * When true, the selection mechanic is currently tracking a marquee rectangle drag, and acting
	 * on the selection may be unwise until it is over (and an OnSelectionChanged event is fired).
	 */
	bool IsCurrentlyMarqueeDragging() { return bCurrentlyMarqueeDragging; }

	// TODO: Is it worth issuing separate callbacks in normal selection changes and in FChange ones, to
	// allow the user to bundle in some FChanges into the normal callback?

	UPROPERTY()
	TObjectPtr<UPolygonSelectionMechanicProperties> Properties;

protected:
	bool bIsEnabled = true;

	const FDynamicMesh3* Mesh;
	const FGroupTopology* Topology;
	TFunction<FDynamicMeshAABBTree3*()> GetSpatialFunc;

	UPROPERTY()
	TObjectPtr<UMouseHoverBehavior> HoverBehavior;

	UPROPERTY()
	TObjectPtr<USingleClickOrDragInputBehavior> ClickOrDragBehavior;

	UPROPERTY()
	TObjectPtr<URectangleMarqueeMechanic> MarqueeMechanic;

	FInputCapturePriority BasePriority = FInputCapturePriority(FInputCapturePriority::DEFAULT_TOOL_PRIORITY);

	// When bSelectEdgeLoops is true, this function is tested to see if we should select edge loops,
	// to allow edge loop selection to be toggled with some key (setting bSelectEdgeLoops to
	// false overrides this function).
	TFunction<bool(void)> ShouldSelectEdgeLoopsFunc = []() {return true; };

	// When bSelectEdgeRings is true, this function is tested to see if we should select edge rings,
	// to allow edge ring selection to be toggled with some key (setting bSelectEdgeRings to
	// false overrides this function).
	TFunction<bool(void)> ShouldSelectEdgeRingsFunc = []() {return true; };

	TFunction<bool(void)> ShouldAddToSelectionFunc = [this]() {return bShiftToggle; };
	TFunction<bool(void)> ShouldRemoveFromSelectionFunc = [this]() {return bCtrlToggle; };;

	FTransform3d TargetTransform;

	TSharedPtr<FGroupTopologySelector, ESPMode::ThreadSafe> TopoSelector;

	/**
	 * Get the topology selector settings to use given the current selection settings.
	 * 
	 * @param bUseOrthoSettings If true, the topology selector will be configured to use ortho settings,
	 *  which are generally different to allow for selection of projected elements, etc.
	 */
	FGroupTopologySelector::FSelectionSettings GetTopoSelectorSettings(bool bUseOrthoSettings = false);

	FGroupTopologySelection HilightSelection;
	FGroupTopologySelection PersistentSelection;
	int32 SelectionTimestamp = 0;
	TUniquePtr<FPolygonSelectionMechanicSelectionChange> ActiveChange;

	// Used for box selection
	FGroupTopologySelection PreDragPersistentSelection;
	FGroupTopologySelection LastUpdateRectangleSelection;
	FGroupTopologySelector::FSelectionSettings PreDragTopoSelectorSettings;
	TMap<int32, bool> TriIsOccludedCache;
	bool bCurrentlyMarqueeDragging = false;

	FVector3d LastClickedHitPosition;
	FVector3d LastClickedHitNormal;

	/** The actor we create internally to own the DrawnTriangleSetComponent */
	UPROPERTY()
	TObjectPtr<APreviewGeometryActor> PreviewGeometryActor;

	UPROPERTY()
	TObjectPtr<UTriangleSetComponent> DrawnTriangleSetComponent;

	TSet<int> CurrentlyHighlightedGroups;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> HighlightedFaceMaterial;

	FViewCameraState CameraState;

	bool bShiftToggle = false;
	bool bCtrlToggle = false;
	static const int32 ShiftModifierID = 1;
	static const int32 CtrlModifierID = 2;

	bool bShowSelectableCorners = true;
public:
	FToolDataVisualizer PolyEdgesRenderer;
	FToolDataVisualizer HilightRenderer;
	FToolDataVisualizer SelectionRenderer;

	friend class FPolygonSelectionMechanicSelectionChange;
};



class MODELINGCOMPONENTS_API FPolygonSelectionMechanicSelectionChange : public FToolCommandChange
{
public:
	FGroupTopologySelection Before;
	FGroupTopologySelection After;
	int32 Timestamp = 0;

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override;
};