// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBrushTool.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "InteractiveToolQueryInterfaces.h"
#include "SelectionSet.h"
#include "Changes/MeshSelectionChange.h"
#include "DynamicMesh/DynamicMeshOctree3.h"
#include "Polygroups/PolygroupSet.h"
#include "MeshSelectionTool.generated.h"

class UMeshStatisticsProperties;
class UMeshElementsVisualizer;
class UMeshUVChannelProperties;
class UPolygroupLayersProperties;
class UMeshSelectionTool;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshSelectionToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};




UENUM()
enum class EMeshSelectionToolActions
{
	NoAction,

	SelectAll,
	ClearSelection,
	InvertSelection,
	GrowSelection,
	ShrinkSelection,
	ExpandToConnected,

	SelectLargestComponentByTriCount,
	SelectLargestComponentByArea,
	OptimizeSelection,

	DeleteSelected,
	DisconnectSelected,
	SeparateSelected,
	DuplicateSelected,
	FlipSelected,
	CreateGroup,
	SmoothBoundary,

	CycleSelectionMode,
	CycleViewMode
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshSelectionToolActionPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UMeshSelectionTool> ParentTool;

	void Initialize(UMeshSelectionTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EMeshSelectionToolActions Action);
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshSelectionEditActions : public UMeshSelectionToolActionPropertySet
{
	GENERATED_BODY()

public:
	/** Clear the active triangle selection */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 0))
	void Clear()
	{
		PostAction(EMeshSelectionToolActions::ClearSelection);
	}

	/** Select all triangles in the mesh */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 1))
	void SelectAll()
	{
		PostAction(EMeshSelectionToolActions::SelectAll);
	}

	/** Invert the active triangle selection */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 2))
	void Invert()
	{
		PostAction(EMeshSelectionToolActions::InvertSelection);
	}

	/** Grow the active triangle selection to include any triangles touching a vertex on the selection boundary */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 3))
	void Grow()
	{
		PostAction(EMeshSelectionToolActions::GrowSelection);
	}

	/** Shrink the active triangle selection by removing any triangles touching a vertex on the selection boundary */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = ( DisplayPriority = 4))
	void Shrink()
	{
		PostAction(EMeshSelectionToolActions::ShrinkSelection);
	}

	/** Grow the active selection to include any triangle connected via shared edges (ie flood-fill) */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 5))
	void FloodFill()
	{
		PostAction(EMeshSelectionToolActions::ExpandToConnected);
	}

	/** Select the largest connected mesh component by triangle count */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 6))
	void LargestTriCountPart()
	{
		PostAction(EMeshSelectionToolActions::SelectLargestComponentByTriCount);
	}

	/** Select the largest connected mesh component by mesh area */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 7))
	void LargestAreaPart()
	{
		PostAction(EMeshSelectionToolActions::SelectLargestComponentByArea);
	}

	/** Optimize the selection border */
	UFUNCTION(CallInEditor, Category = SelectionEdits, meta = (DisplayPriority = 8))
	void OptimizeBorder()
	{
		PostAction(EMeshSelectionToolActions::OptimizeSelection);
	}
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshSelectionMeshEditActions : public UMeshSelectionToolActionPropertySet
{
	GENERATED_BODY()

public:
	/** Delete the selected triangles */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayName = "Delete", DisplayPriority = 1))
	void Delete()
	{
		PostAction(EMeshSelectionToolActions::DeleteSelected);
	}

	/** Disconnected the selected triangles from their neighbours, to create mesh boundaries along the selection borders */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayName = "Disconnect", DisplayPriority = 3))
	void Disconnect() 
	{
		PostAction(EMeshSelectionToolActions::DisconnectSelected);
	}

	/** Flip the normals of the selected triangles. This will create hard normals at selection borders. */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayName = "Flip Normals", DisplayPriority = 4))
	void FlipNormals() 
	{
		PostAction(EMeshSelectionToolActions::FlipSelected);
	}

	/** Assign a new unique Polygroup index to the selected triangles */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayName = "Create Polygroup", DisplayPriority = 5))
	void CreatePolygroup()
	{
		PostAction(EMeshSelectionToolActions::CreateGroup);
	}

	/** Delete the selected triangles from the active Mesh Object and create a new Mesh Object containing those triangles */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayName = "Separate", DisplayPriority = 10))
	void Separate() 
	{
		PostAction(EMeshSelectionToolActions::SeparateSelected);
	}

	/** Create a new Mesh Object containing the selected triangles */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayName = "Duplicate", DisplayPriority = 11))
	void Duplicate() 
	{
		PostAction(EMeshSelectionToolActions::DuplicateSelected);
	}

	/** Smooth the selection border */
	UFUNCTION(CallInEditor, Category = MeshEdits, meta = (DisplayPriority = 12))
	void SmoothBorder()
	{
		PostAction(EMeshSelectionToolActions::SmoothBoundary);
	}

};






UENUM()
enum class EMeshSelectionToolPrimaryMode
{
	/** Select all triangles inside the brush area */
	Brush,

	/** Select all triangles inside the brush volume */
	VolumetricBrush,

	/** Select all triangles inside brush with normal within angular tolerance of hit triangle */
	AngleFiltered,

	/** Select all triangles inside brush that are visible from current view */
	Visible,

	/** Select all triangles connected to any triangle inside the brush */
	AllConnected,

	/** Select all triangles in groups connected to any triangle inside the brush */
	AllInGroup,

	/** Select all triangles with same material as hit triangle */
	ByMaterial,

	/** Select all triangles in same UV island as hit triangle */
	ByUVIsland,

	/** Select all triangles with normal within angular tolerance of hit triangle */
	AllWithinAngle
};



UENUM()
enum class EMeshFacesColorMode
{
	/** Display original mesh materials */
	None,
	/** Color mesh triangles by PolyGroup Color */
	ByGroup,
	/** Color mesh triangles by Material ID */
	ByMaterialID,
	/** Color mesh triangles by UV Island */
	ByUVIsland
};


UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshSelectionToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** The Selection Mode defines the behavior of the selection brush */
	UPROPERTY(EditAnywhere, Category = Selection)
	EMeshSelectionToolPrimaryMode SelectionMode = EMeshSelectionToolPrimaryMode::Brush;

	/** Angle in Degrees used for Angle-based Selection Modes */
	UPROPERTY(EditAnywhere, Category = Selection, meta = (UIMin = "0.0", UIMax = "90.0") )
	float AngleTolerance = 1.0;

	/** Allow the brush to hit back-facing parts of the surface  */
	UPROPERTY(EditAnywhere, Category = Selection)
	bool bHitBackFaces = true;

	/** Toggle drawing of highlight points on/off */
	UPROPERTY(EditAnywhere, Category = Selection)
	bool bShowPoints = false;

	/** Color each triangle based on the selected mesh attribute */
	UPROPERTY(EditAnywhere, Category = Selection)
	EMeshFacesColorMode FaceColorMode = EMeshFacesColorMode::None;
};



/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshSelectionTool : public UDynamicMeshBrushTool, public IInteractiveToolNestedAcceptCancelAPI
{
	GENERATED_BODY()

public:
	UMeshSelectionTool();

	virtual void SetWorld(UWorld* World);

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return Super::CanAccept() && bHaveModifiedMesh; }

	// UBaseBrushTool overrides
	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

	// IInteractiveToolCameraFocusAPI implementation
	virtual FBox GetWorldSpaceFocusBox() override;

	// IInteractiveToolCancelAPI
	virtual bool SupportsNestedCancelCommand() override;
	virtual bool CanCurrentlyNestedCancel() override;
	virtual bool ExecuteNestedCancelCommand() override;

public:

	virtual void RequestAction(EMeshSelectionToolActions ActionType);

	UPROPERTY()
	TObjectPtr<UMeshSelectionToolProperties> SelectionProps;

	UPROPERTY()
	TObjectPtr<UMeshSelectionEditActions> SelectionActions;

	UPROPERTY()
	TObjectPtr<UMeshSelectionToolActionPropertySet> EditActions;

	UPROPERTY()
	TObjectPtr<UMeshStatisticsProperties> MeshStatisticsProperties;

	UPROPERTY()
	TObjectPtr<UMeshElementsVisualizer> MeshElementsDisplay;

	UPROPERTY()
	TObjectPtr<UMeshUVChannelProperties> UVChannelProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UPolygroupLayersProperties> PolygroupLayerProperties = nullptr;

protected:
	virtual UMeshSelectionToolActionPropertySet* CreateEditActions();
	virtual void AddSubclassPropertySets() {}

protected:

	virtual void ApplyStamp(const FBrushStampData& Stamp);

	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

protected:
	UPROPERTY()
	TObjectPtr<UMeshSelectionSet> Selection;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UWorld* TargetWorld;

	// note: ideally this octree would be part of PreviewMesh!
	TUniquePtr<UE::Geometry::FDynamicMeshOctree3> Octree;
	bool bOctreeValid = false;
	TUniquePtr<UE::Geometry::FDynamicMeshOctree3>& GetOctree();

	EMeshSelectionElementType SelectionType = EMeshSelectionElementType::Face;

	bool bInRemoveStroke = false;
	FBrushStampData StartStamp;
	FBrushStampData LastStamp;
	bool bStampPending;

	void UpdateFaceSelection(const FBrushStampData& Stamp, const TArray<int>& BrushROI);


	// temp
	TArray<int> IndexBuf;
	TArray<int32> TemporaryBuffer;
	TSet<int32> TemporarySet;
	void CalculateVertexROI(const FBrushStampData& Stamp, TArray<int>& VertexROI);
	void CalculateTriangleROI(const FBrushStampData& Stamp, TArray<int>& TriangleROI);
	TArray<int> PreviewBrushROI;
	TBitArray<> SelectedVertices;
	TBitArray<> SelectedTriangles;
	void OnExternalSelectionChange();

	void OnRegionHighlightUpdated(const TArray<int32>& Triangles);
	void OnRegionHighlightUpdated(const TSet<int32>& Triangles);
	void OnRegionHighlightUpdated();
	void UpdateVisualization(bool bSelectionModified);
	bool bFullMeshInvalidationPending = false;
	bool bColorsUpdatePending = false;
	FColor GetCurrentFaceColor(const FDynamicMesh3* Mesh, int TriangleID);
	void CacheUVIslandIDs();
	TArray<int> TriangleToUVIsland;

	// selection change
	FMeshSelectionChangeBuilder* ActiveSelectionChange = nullptr;
	void BeginChange(bool bAdding);
	TUniquePtr<FToolCommandChange> EndChange();
	void CancelChange();


	// actions

	bool bHavePendingAction = false;
	EMeshSelectionToolActions PendingAction;
	virtual void ApplyAction(EMeshSelectionToolActions ActionType);

	void SelectAll();
	void ClearSelection();
	void InvertSelection();
	void GrowShrinkSelection(bool bGrow);
	void ExpandToConnected();

	void SelectLargestComponent(bool bWeightByArea);
	void OptimizeSelection();

	void DeleteSelectedTriangles();
	void DisconnectSelectedTriangles(); // disconnects edges between selected and not-selected triangles; keeps all triangles in the same mesh
	void SeparateSelectedTriangles(bool bDeleteSelected); // separates out selected triangles to a new mesh, optionally removing them from the current mesh
	void FlipSelectedTriangles();
	void AssignNewGroupToSelectedTriangles();

	void SmoothSelectionBoundary();

	TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> ActiveGroupSet;
	void OnSelectedGroupLayerChanged();
	void UpdateActiveGroupLayer();

	// if true, mesh has been edited
	bool bHaveModifiedMesh = false;

	virtual void ApplyShutdownAction(EToolShutdownType ShutdownType);
};



