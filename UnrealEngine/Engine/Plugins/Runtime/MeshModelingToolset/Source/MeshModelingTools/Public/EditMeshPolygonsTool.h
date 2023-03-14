// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Changes/MeshVertexChange.h" // IMeshVertexCommandChangeTarget
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshChangeTracker.h" // FDynamicMeshChange for TUniquePtr
#include "InteractiveToolActivity.h" // IToolActivityHost
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h" // IInteractiveToolNestedAcceptCancelAPI
#include "Operations/GroupTopologyDeformer.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"

#include "GeometryBase.h"

#include "EditMeshPolygonsTool.generated.h"

PREDECLARE_GEOMETRY(class FGroupTopology);
PREDECLARE_GEOMETRY(struct FGroupTopologySelection);

class UCombinedTransformGizmo;
class UDragAlignmentMechanic;
class UMeshOpPreviewWithBackgroundCompute; 
class FMeshVertexChangeBuilder;
class UPersistentMeshSelection;
class UEditMeshPolygonsTool;
class UPolyEditActivityContext;
class UPolyEditInsertEdgeActivity;
class UPolyEditInsertEdgeLoopActivity;
class UPolyEditExtrudeActivity;
class UPolyEditInsetOutsetActivity;
class UPolyEditCutFacesActivity;
class UPolyEditPlanarProjectionUVActivity;
class UPolyEditBevelEdgeActivity;
class UPolygonSelectionMechanic;
class UTransformProxy;


/**
 * ToolBuilder
 */
UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	bool bTriangleMode = false;

	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
	virtual void InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const override;

	virtual bool WantsInputSelectionIfAvailable() const { return true; }
};


UENUM()
enum class ELocalFrameMode
{
	FromObject,
	FromGeometry
};


/** 
 * These are properties that do not get enabled/disabled based on the action 
 */
UCLASS()
class MESHMODELINGTOOLS_API UPolyEditCommonProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowWireframe = false;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowSelectableCorners = true;

	/** When true, allows the transform gizmo to be rendered */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bGizmoVisible = true;

	/** Determines whether, on selection changes, the gizmo's rotation is taken from the object transform, or from the geometry
	 elements selected. Only relevant with a local coordinate system and when rotation is not locked. */
	UPROPERTY(EditAnywhere, Category = Gizmo, meta = (HideEditConditionToggle, EditCondition = "bLocalCoordSystem && !bLockRotation"))
	ELocalFrameMode LocalFrameMode = ELocalFrameMode::FromGeometry;

	/** When true, keeps rotation of gizmo constant through selection changes and manipulations 
	 (but not middle-click repositions). Only active with a local coordinate system.*/
	UPROPERTY(EditAnywhere, Category = Gizmo, meta = (HideEditConditionToggle, EditCondition = "bLocalCoordSystem"))
	bool bLockRotation = false;

	/** This gets updated internally so that properties can respond to whether the coordinate system is set to local or global */
	UPROPERTY()
	bool bLocalCoordSystem = true;
};


UENUM()
enum class EEditMeshPolygonsToolActions
{
	NoAction,
	AcceptCurrent,
	CancelCurrent,
	Extrude,
	PushPull,
	Offset,
	Inset,
	Outset,
	BevelFaces,
	InsertEdge,
	InsertEdgeLoop,
	Complete,

	PlaneCut,
	Merge,
	Delete,
	CutFaces,
	RecalculateNormals,
	FlipNormals,
	Retriangulate,
	Decompose,
	Disconnect,
	Duplicate,

	CollapseEdge,
	WeldEdges,
	StraightenEdge,
	FillHole,
	BridgeEdges,
	BevelEdges,

	PlanarProjectionUV,

	SimplifyByGroups,
	RegenerateExtraCorners,

	// triangle-specific edits
	PokeSingleFace,
	SplitSingleEdge,
	FlipSingleEdge,
	CollapseSingleEdge
};

UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsActionModeToolBuilder : public UEditMeshPolygonsToolBuilder
{
	GENERATED_BODY()
public:
	EEditMeshPolygonsToolActions StartupAction = EEditMeshPolygonsToolActions::Extrude;

	virtual void InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const override;
};

UENUM()
enum class EEditMeshPolygonsToolSelectionMode
{
	Faces,
	Edges,
	Vertices,
	Loops,
	Rings,
	FacesEdgesVertices
};

UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsSelectionModeToolBuilder : public UEditMeshPolygonsToolBuilder
{
	GENERATED_BODY()
public:
	EEditMeshPolygonsToolSelectionMode SelectionMode = EEditMeshPolygonsToolSelectionMode::Faces;

	virtual void InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const override;
};



UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolActionPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UEditMeshPolygonsTool> ParentTool;

	void Initialize(UEditMeshPolygonsTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EEditMeshPolygonsToolActions Action);
};

UCLASS()
class MESHMODELINGTOOLS_API UPolyEditTopologyProperties : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()

public:
	/** 
	 * When true, adds extra corners at sharp group edge bends (in addition to the normal corners that
	 * are placed at junctures of three or more group edges). For instance, a single disconnected quad-like group
	 * would normally have a single group edge with no corners, since it has no neighboring groups, but this
	 * setting will allow for the generation of corners at the quad corners, which is very useful for editing.
	 * Note that the setting takes effect only after clicking Regenerate Extra Corners or performing some
	 * operation that changes the group topology.
	 */
	UPROPERTY(EditAnywhere, Category = TopologyOptions)
	bool bAddExtraCorners = true;

	UFUNCTION(CallInEditor, Category = TopologyOptions)
	void RegenerateExtraCorners() { PostAction(EEditMeshPolygonsToolActions::RegenerateExtraCorners); }

	/** 
	 * When generating extra corners, how sharp the angle needs to be to warrant an extra corner placement there. Lower values require
	 * sharper corners, so are more tolerant of curved group edges. For instance, 180 will place corners at every vertex along a group
	 * edge even if the edge is perfectly straight, and 135 will place a vertex only once the edge bends 45 degrees off the straight path
	 * (i.e. 135 degrees to the previous edge). 
	 * The setting is applied either when Regenerate Extra Corners is clicked, or after any operation that modifies topology.
	 */
	UPROPERTY(EditAnywhere, Category = TopologyOptions, meta = (ClampMin = "0", ClampMax = "180", EditCondition = "bAddExtraCorners"))
	double ExtraCornerAngleThresholdDegrees = 135;
};

/** PolyEdit Actions */
UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolActions : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()
public:
	/** Extrude the current set of selected faces by moving and stitching them. */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Extrude", DisplayPriority = 1))
	void Extrude() { PostAction(EEditMeshPolygonsToolActions::Extrude); }

	/** Like Extrude/Offset, but performed in a boolean way, meaning that the faces can cut away the mesh or bridge mesh parts. */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Push/Pull", DisplayPriority = 1))
	void PushPull() { PostAction(EEditMeshPolygonsToolActions::PushPull); }

	/** Like Extrude, but defaults to moving verts along vertex normals instead of a single direction. */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Offset", DisplayPriority = 1))
	void Offset() { PostAction(EEditMeshPolygonsToolActions::Offset); }

	/** Inset the current set of selected faces. Click in viewport to confirm inset distance. */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Inset", DisplayPriority = 2))
	void Inset() { PostAction(EEditMeshPolygonsToolActions::Inset);	}

	/** Outset the current set of selected faces. Click in viewport to confirm outset distance. */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Outset", DisplayPriority = 3))
	void Outset() { PostAction(EEditMeshPolygonsToolActions::Outset);	}

	//~ TODO: Make the Merge, Delete, and Flip comments visible as tooltips. Currently we can't due to a bug that
	//~ limits our total tooltip text allotment: UE-124608

	//~ Bevel the edge loops around the selected faces 
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Bevel", DisplayPriority = 4))
	void Bevel() { PostAction(EEditMeshPolygonsToolActions::BevelFaces); }

	//~ Merge the current set of selected faces into a single face.
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Merge", DisplayPriority = 4))
	void Merge() { PostAction(EEditMeshPolygonsToolActions::Merge);	}

	//~ Delete the current set of selected faces
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Delete", DisplayPriority = 4))
	void Delete() { PostAction(EEditMeshPolygonsToolActions::Delete); }

	/** Cut the current set of selected faces. Click twice in viewport to set cut line. */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "CutFaces", DisplayPriority = 5))
	void CutFaces() { PostAction(EEditMeshPolygonsToolActions::CutFaces); }

	/** Recalculate normals for the current set of selected faces */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "RecalcNormals", DisplayPriority = 6))
	void RecalcNormals() { PostAction(EEditMeshPolygonsToolActions::RecalculateNormals); }

	//~ Flip normals and face orientation for the current set of selected faces
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Flip", DisplayPriority = 7))
	void Flip() { PostAction(EEditMeshPolygonsToolActions::FlipNormals); }

	/** Retriangulate each of the selected faces */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Retriangulate", DisplayPriority = 9))
	void Retriangulate() { PostAction(EEditMeshPolygonsToolActions::Retriangulate);	}

	/** Split each of the selected faces into a separate polygon for each triangle */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Decompose", DisplayPriority = 10))
	void Decompose() { PostAction(EEditMeshPolygonsToolActions::Decompose);	}

	/** Separate the selected faces at their borders */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Disconnect", DisplayPriority = 11))
	void Disconnect() { PostAction(EEditMeshPolygonsToolActions::Disconnect); }

	/** Duplicate the selected faces at their borders */
	UFUNCTION(CallInEditor, Category = FaceEdits, meta = (DisplayName = "Duplicate", DisplayPriority = 12))
	void Duplicate() { PostAction(EEditMeshPolygonsToolActions::Duplicate); }

	//~ TODO: add tooltip, especially explaining limitations, for this and InsertEdge. Can't currently do that
	//~ without cutting down another tooltip until UE-124608 is fixed...
	UFUNCTION(CallInEditor, Category = ShapeEdits, meta = (DisplayName = "InsertEdgeLoop", DisplayPriority = 13))
	void InsertEdgeLoop() { PostAction(EEditMeshPolygonsToolActions::InsertEdgeLoop); }

	UFUNCTION(CallInEditor, Category = ShapeEdits, meta = (DisplayName = "Insert Edge", DisplayPriority = 14))
	void InsertEdge() { PostAction(EEditMeshPolygonsToolActions::InsertEdge); }

	/** Simplify every polygon group by removing vertices on shared straight edges and retriangulating */
	UFUNCTION(CallInEditor, Category = ShapeEdits, meta = (DisplayName = "SimplifyByGroups", DisplayPriority = 15))
	void SimplifyByGroups() { PostAction(EEditMeshPolygonsToolActions::SimplifyByGroups); }

};



UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolActions_Triangles : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()
public:
	/** Extrude the current set of selected faces. Click in viewport to confirm extrude height. */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Extrude", DisplayPriority = 1))
	void Extrude() { PostAction(EEditMeshPolygonsToolActions::Extrude); }

	/** Like Extrude/Offset, but performed in a boolean way, meaning that the faces can cut away the mesh or bridge mesh parts. */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Push/Pull", DisplayPriority = 1))
	void PushPull() { PostAction(EEditMeshPolygonsToolActions::PushPull); }

	/** Like Extrude, but defaults to moving verts along vertex normals instead of a single direction. */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Offset", DisplayPriority = 1))
	void Offset() { PostAction(EEditMeshPolygonsToolActions::Offset); }

	/** Inset the current set of selected faces. Click in viewport to confirm inset distance. */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Inset", DisplayPriority = 2))
	void Inset() { PostAction(EEditMeshPolygonsToolActions::Inset);	}

	/** Outset the current set of selected faces. Click in viewport to confirm outset distance. */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Outset", DisplayPriority = 3))
	void Outset() { PostAction(EEditMeshPolygonsToolActions::Outset);	}

	/** Delete the current set of selected faces */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Delete", DisplayPriority = 4))
	void Delete() { PostAction(EEditMeshPolygonsToolActions::Delete); }

	/** Cut the current set of selected faces. Click twice in viewport to set cut line. */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "CutFaces", DisplayPriority = 5))
	void CutFaces() { PostAction(EEditMeshPolygonsToolActions::CutFaces); }

	/** Recalculate normals for the current set of selected faces */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "RecalcNormals", DisplayPriority = 6))
	void RecalcNormals() { PostAction(EEditMeshPolygonsToolActions::RecalculateNormals); }

	/** Flip normals and face orientation for the current set of selected faces */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Flip", DisplayPriority = 7))
	void Flip() { PostAction(EEditMeshPolygonsToolActions::FlipNormals); }

	/** Separate the selected faces at their borders */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Disconnect", DisplayPriority = 11))
	void Disconnect() { PostAction(EEditMeshPolygonsToolActions::Disconnect); }

	/** Duplicate the selected faces */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Duplicate", DisplayPriority = 12))
	void Duplicate() { PostAction(EEditMeshPolygonsToolActions::Duplicate); }

	/** Poke each face at its center point */
	UFUNCTION(CallInEditor, Category = TriangleEdits, meta = (DisplayName = "Poke", DisplayPriority = 13))
	void Poke() { PostAction(EEditMeshPolygonsToolActions::PokeSingleFace); }
};





UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolUVActions : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()

public:

	/** Assign planar-projection UVs to mesh */
	UFUNCTION(CallInEditor, Category = UVs, meta = (DisplayName = "PlanarProjection", DisplayPriority = 11))
	void PlanarProjection()
	{
		PostAction(EEditMeshPolygonsToolActions::PlanarProjectionUV);
	}
};





UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolEdgeActions : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Weld", DisplayPriority = 1))
	void Weld() { PostAction(EEditMeshPolygonsToolActions::WeldEdges); }

	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Straighten", DisplayPriority = 2))
	void Straighten() { PostAction(EEditMeshPolygonsToolActions::StraightenEdge); }

	/** Fill the adjacent hole for any selected boundary edges */
	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Fill Hole", DisplayPriority = 3))
	void FillHole()	{ PostAction(EEditMeshPolygonsToolActions::FillHole); }

	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Bevel", DisplayPriority = 4))
	void Bevel() { PostAction(EEditMeshPolygonsToolActions::BevelEdges); }
	
	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Bridge", DisplayPriority = 5))
	void Bridge() { PostAction(EEditMeshPolygonsToolActions::BridgeEdges); }
};


UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolEdgeActions_Triangles : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Weld", DisplayPriority = 1))
	void Weld() { PostAction(EEditMeshPolygonsToolActions::WeldEdges); }

	/** Fill the adjacent hole for any selected boundary edges */
	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Fill Hole", DisplayPriority = 1))
	void FillHole() { PostAction(EEditMeshPolygonsToolActions::FillHole); }

	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Collapse", DisplayPriority = 1))
	void Collapse() { PostAction(EEditMeshPolygonsToolActions::CollapseSingleEdge); }

	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Flip", DisplayPriority = 1))
	void Flip() { PostAction(EEditMeshPolygonsToolActions::FlipSingleEdge); }

	UFUNCTION(CallInEditor, Category = EdgeEdits, meta = (DisplayName = "Split", DisplayPriority = 1))
	void Split() { PostAction(EEditMeshPolygonsToolActions::SplitSingleEdge); }

};


/**
 * TODO: This is currently a separate action set so that we can show/hide it depending on whether
 * we have an activity running. We should have a cleaner alternative.
 */
UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolCancelAction : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category = CurrentOperation, meta = (DisplayName = "Cancel", DisplayPriority = 1))
	void Done() { PostAction(EEditMeshPolygonsToolActions::CancelCurrent); }
};


/**
 * TODO: This is currently a separate action set so that we can show/hide it depending on whether
 * we have an activity running. We should have a cleaner alternative.
 */
UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsToolAcceptCancelAction : public UEditMeshPolygonsToolActionPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category = CurrentOperation, meta = (DisplayName = "Apply", DisplayPriority = 1))
	void Apply() { PostAction(EEditMeshPolygonsToolActions::AcceptCurrent); }

	UFUNCTION(CallInEditor, Category = CurrentOperation, meta = (DisplayName = "Cancel", DisplayPriority = 2))
	void Cancel() { PostAction(EEditMeshPolygonsToolActions::CancelCurrent); }
};




/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UEditMeshPolygonsTool : public USingleSelectionMeshEditingTool,
	public IToolActivityHost, 
	public IMeshVertexCommandChangeTarget,
	public IInteractiveToolNestedAcceptCancelAPI
{
	GENERATED_BODY()
	using FFrame3d = UE::Geometry::FFrame3d;

public:
	UEditMeshPolygonsTool();

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	void EnableTriangleMode();

	virtual void SetWorld(UWorld* World) { this->TargetWorld = World; }

	// used by undo/redo
	void RebuildTopologyWithGivenExtraCorners(const TSet<int32>& Vids);

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IInteractiveToolCameraFocusAPI implementation
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

	// IToolActivityHost
	virtual void NotifyActivitySelfEnded(UInteractiveToolActivity* Activity) override;

	// IMeshVertexCommandChangeTarget
	virtual void ApplyChange(const FMeshVertexChange* Change, bool bRevert) override;

	// IInteractiveToolNestedAcceptCancelAPI
	virtual bool SupportsNestedCancelCommand() override { return true; }
	virtual bool CanCurrentlyNestedCancel() override;
	virtual bool ExecuteNestedCancelCommand() override;
	virtual bool SupportsNestedAcceptCommand() override { return true; }
	virtual bool CanCurrentlyNestedAccept() override;
	virtual bool ExecuteNestedAcceptCommand() override;

public:

	virtual void RequestAction(EEditMeshPolygonsToolActions ActionType);

	void SetActionButtonsVisibility(bool bVisible);

protected:
	// If bTriangleMode = true, then we use a per-triangle FTriangleGroupTopology instead of polygroup topology.
	// This allows low-level mesh editing with mainly the same code, at a significant cost in overhead.
	// This is a fundamental mode switch, must be set before ::Setup() is called!
	bool bTriangleMode;		

	// TODO: This is a hack to allow us to disallow any actions inside the tool after Setup() is called. We
	// use it if the user tries to run the tool on a mesh that has too many edges for us to render, to avoid
	// hanging the editor.
	bool bToolDisabled = false;

	TObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview = nullptr;

	UPROPERTY()
	TObjectPtr<UPolyEditCommonProperties> CommonProps = nullptr;

	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolActions> EditActions = nullptr;
	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolActions_Triangles> EditActions_Triangles = nullptr;

	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolEdgeActions> EditEdgeActions = nullptr;
	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolEdgeActions_Triangles> EditEdgeActions_Triangles = nullptr;

	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolUVActions> EditUVActions = nullptr;

	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolCancelAction> CancelAction = nullptr;

	UPROPERTY()
	TObjectPtr<UEditMeshPolygonsToolAcceptCancelAction> AcceptCancelAction = nullptr;

	UPROPERTY()
	TObjectPtr<UPolyEditTopologyProperties> TopologyProperties = nullptr;

	/**
	 * Activity objects that handle multi-interaction operations
	 */
	UPROPERTY()
	TObjectPtr<UPolyEditExtrudeActivity> ExtrudeActivity = nullptr;
	UPROPERTY()
	TObjectPtr<UPolyEditInsetOutsetActivity> InsetOutsetActivity = nullptr;
	UPROPERTY()
	TObjectPtr<UPolyEditCutFacesActivity> CutFacesActivity = nullptr;
	UPROPERTY()
	TObjectPtr<UPolyEditPlanarProjectionUVActivity> PlanarProjectionUVActivity = nullptr;
	UPROPERTY()
	TObjectPtr<UPolyEditInsertEdgeActivity> InsertEdgeActivity = nullptr;
	UPROPERTY()
	TObjectPtr<UPolyEditInsertEdgeLoopActivity> InsertEdgeLoopActivity = nullptr;
	UPROPERTY()
	TObjectPtr<UPolyEditBevelEdgeActivity> BevelEdgeActivity = nullptr;

	/**
	 * Points to one of the activities when it is active
	 */
	TObjectPtr<UInteractiveToolActivity> CurrentActivity = nullptr;

	TSharedPtr<UE::Geometry::FDynamicMesh3> CurrentMesh;
	TSharedPtr<UE::Geometry::FGroupTopology> Topology;
	TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3> MeshSpatial;

	UPROPERTY()
	TObjectPtr<UPolyEditActivityContext> ActivityContext = nullptr;

	UPROPERTY()
	TObjectPtr<UPolygonSelectionMechanic> SelectionMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UDragAlignmentMechanic> DragAlignmentMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> TransformGizmo = nullptr;

	UPROPERTY()
	TObjectPtr<UTransformProxy> TransformProxy = nullptr;

	FText DefaultMessage;

	void ResetUserMessage();

	bool IsToolInputSelectionUsable(const UPersistentMeshSelection* InputSelection);
	bool bSelectionStateDirty = false;
	void OnSelectionModifiedEvent();

	void OnBeginGizmoTransform(UTransformProxy* Proxy);
	void OnEndGizmoTransform(UTransformProxy* Proxy);
	void OnGizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform);
	void UpdateGizmoFrame(const FFrame3d* UseFrame = nullptr);
	FFrame3d LastGeometryFrame;
	FFrame3d LastTransformerFrame;
	FFrame3d LockedTransfomerFrame;
	bool bInGizmoDrag = false;

	UE::Geometry::FTransformSRT3d BakedTransform; // We bake the scale part of the Target -> World transform
	UE::Geometry::FTransformSRT3d WorldTransform; // Transform from Baked to World

	FFrame3d InitialGizmoFrame;
	FVector3d InitialGizmoScale;
	bool bGizmoUpdatePending = false;
	FFrame3d LastUpdateGizmoFrame;
	FVector3d LastUpdateGizmoScale;
	bool bLastUpdateUsedWorldFrame = false;
	void ComputeUpdate_Gizmo();

	UE::Geometry::FDynamicMeshAABBTree3& GetSpatial();
	bool bSpatialDirty;

	// UV Scale factor to apply to texturing on any new geometry (e.g. new faces added by extrude)
	float UVScaleFactor = 1.0f;

	EEditMeshPolygonsToolActions PendingAction = EEditMeshPolygonsToolActions::NoAction;

	int32 ActivityTimestamp = 1;

	void StartActivity(TObjectPtr<UInteractiveToolActivity> Activity);
	void EndCurrentActivity(EToolShutdownType ShutdownType = EToolShutdownType::Cancel);
	void SetActionButtonPanelsVisible(bool bVisible);

	// Emit an undoable change to CurrentMesh and update related structures (preview, spatial, etc)
	void EmitCurrentMeshChangeAndUpdate(const FText& TransactionLabel,
		TUniquePtr<UE::Geometry::FDynamicMeshChange> MeshChangeIn,
		const UE::Geometry::FGroupTopologySelection& OutputSelection);

	// Emit an undoable start of an activity
	void EmitActivityStart(const FText& TransactionLabel);

	void UpdateGizmoVisibility();

	void ApplyMerge();
	void ApplyDelete();
	void ApplyRecalcNormals();
	void ApplyFlipNormals();
	void ApplyRetriangulate();
	void ApplyDecompose();
	void ApplyDisconnect();
	void ApplyDuplicate();
	void ApplyPokeSingleFace();

	void ApplyCollapseEdge();
	void ApplyWeldEdges();
	void ApplyStraightenEdges();
	void ApplyFillHole();
	void ApplyBridgeEdges();

	void ApplyFlipSingleEdge();
	void ApplyCollapseSingleEdge();
	void ApplySplitSingleEdge();

	void SimplifyByGroups();

	void ApplyRegenerateExtraCorners();
	double ExtraCornerDotProductThreshold = -1;

	FFrame3d ActiveSelectionFrameLocal;
	FFrame3d ActiveSelectionFrameWorld;
	TArray<int32> ActiveTriangleSelection;
	UE::Geometry::FAxisAlignedBox3d ActiveSelectionBounds;

	struct FSelectedEdge
	{
		int32 EdgeTopoID;
		TArray<int32> EdgeIDs;
	};
	TArray<FSelectedEdge> ActiveEdgeSelection;

	enum class EPreviewMaterialType
	{
		SourceMaterials, PreviewMaterial, UVMaterial
	};
	void UpdateEditPreviewMaterials(EPreviewMaterialType MaterialType);
	EPreviewMaterialType CurrentPreviewMaterial;


	//
	// data for current drag
	//
	UE::Geometry::FGroupTopologyDeformer LinearDeformer;
	void UpdateDeformerFromSelection(const UE::Geometry::FGroupTopologySelection& Selection);

	FMeshVertexChangeBuilder* ActiveVertexChange;
	void UpdateDeformerChangeFromROI(bool bFinal);
	void BeginDeformerChange();
	void EndDeformerChange();

	bool BeginMeshFaceEditChange();

	bool BeginMeshEdgeEditChange();
	bool BeginMeshBoundaryEdgeEditChange(bool bOnlySimple);
	bool BeginMeshEdgeEditChange(TFunctionRef<bool(int32)> GroupEdgeIDFilterFunc);

	void UpdateFromCurrentMesh(bool bRebuildTopology);
	int32 ModifiedTopologyCounter = 0;
	bool bWasTopologyEdited = false;

	friend class FEditMeshPolygonsToolMeshChange;
	friend class FPolyEditActivityStartChange;

	// custom setup support
	friend class UEditMeshPolygonsSelectionModeToolBuilder;
	friend class UEditMeshPolygonsActionModeToolBuilder;
	TUniqueFunction<void(UEditMeshPolygonsTool*)> PostSetupFunction;
	void SetToSelectionModeInterface();
};


/**
 * Wraps a FDynamicMeshChange so that it can be expired and so that other data
 * structures in the tool can be updated. On apply/revert, the topology is rebuilt
 * using stored extra corner vids.
 */
class MESHMODELINGTOOLS_API FEditMeshPolygonsToolMeshChange : public FToolCommandChange
{
public:
	FEditMeshPolygonsToolMeshChange(TUniquePtr<UE::Geometry::FDynamicMeshChange> MeshChangeIn)
		: MeshChange(MoveTemp(MeshChangeIn))
	{};

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override;

	TSet<int32> ExtraCornerVidsBefore;
	TSet<int32> ExtraCornerVidsAfter;
protected:
	TUniquePtr<UE::Geometry::FDynamicMeshChange> MeshChange;
};



/**
 * FPolyEditActivityStartChange is used to cancel out of an active action on Undo. 
 * No action is taken on Redo, ie we do not re-start the Tool on Redo.
 */
class MESHMODELINGTOOLS_API FPolyEditActivityStartChange : public FToolCommandChange
{
public:
	FPolyEditActivityStartChange(int32 ActivityTimestampIn)
	{
		ActivityTimestamp = ActivityTimestampIn;
	}
	virtual void Apply(UObject* Object) override {}
	virtual void Revert(UObject* Object) override;
	virtual bool HasExpired(UObject* Object) const override;
	virtual FString ToString() const override;

protected:
	bool bHaveDoneUndo = false;
	int32 ActivityTimestamp = 0;
};
