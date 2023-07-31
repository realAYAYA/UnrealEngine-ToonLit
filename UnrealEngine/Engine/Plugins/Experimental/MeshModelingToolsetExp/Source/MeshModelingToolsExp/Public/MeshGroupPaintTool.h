// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/BaseBrushTool.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "Components/DynamicMeshComponent.h"
#include "PropertySets/PolygroupLayersProperties.h"
#include "Mechanics/PolyLassoMarqueeMechanic.h"

#include "Sculpting/MeshSculptToolBase.h"
#include "Sculpting/MeshBrushOpBase.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshOctree3.h"
#include "DynamicMesh/MeshNormals.h"
#include "TransformTypes.h"
#include "Changes/MeshPolygroupChange.h"
#include "Polygroups/PolygroupSet.h"

#include "MeshGroupPaintTool.generated.h"

class UMeshElementsVisualizer;
class UGroupEraseBrushOpProps;
class UGroupPaintBrushOpProps;

DECLARE_STATS_GROUP(TEXT("GroupPaintTool"), STATGROUP_GroupPaintTool, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_UpdateROI"), GroupPaintTool_UpdateROI, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_ApplyStamp"), GroupPaintToolApplyStamp, STATGROUP_GroupPaintTool );
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick"), GroupPaintToolTick, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick_ApplyStampBlock"), GroupPaintTool_Tick_ApplyStampBlock, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick_ApplyStamp_Remove"), GroupPaintTool_Tick_ApplyStamp_Remove, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick_ApplyStamp_Insert"), GroupPaintTool_Tick_ApplyStamp_Insert, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick_NormalsBlock"), GroupPaintTool_Tick_NormalsBlock, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick_UpdateMeshBlock"), GroupPaintTool_Tick_UpdateMeshBlock, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Tick_UpdateTargetBlock"), GroupPaintTool_Tick_UpdateTargetBlock, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Normals_Collect"), GroupPaintTool_Normals_Collect, STATGROUP_GroupPaintTool);
DECLARE_CYCLE_STAT(TEXT("GroupPaintTool_Normals_Compute"), GroupPaintTool_Normals_Compute, STATGROUP_GroupPaintTool);





/**
 * Tool Builder
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshGroupPaintToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};




/** Mesh Sculpting Brush Types */
UENUM()
enum class EMeshGroupPaintInteractionType : uint8
{
	Brush,
	Fill,
	GroupFill,
	PolyLasso,

	LastValue UMETA(Hidden)
};







/** Mesh Sculpting Brush Types */
UENUM()
enum class EMeshGroupPaintBrushType : uint8
{
	/** Paint active group */
	Paint UMETA(DisplayName = "Paint"),

	/** Erase active group */
	Erase UMETA(DisplayName = "Erase"),

	LastValue UMETA(Hidden)
};


/** Mesh Sculpting Brush Area Types */
UENUM()
enum class EMeshGroupPaintBrushAreaType : uint8
{
	Connected,
	Volumetric
};

/** Mesh Sculpting Brush Types */
UENUM()
enum class EMeshGroupPaintVisibilityType : uint8
{
	None,
	FrontFacing,
	Unoccluded
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UGroupPaintBrushFilterProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Primary Brush Mode */
	//UPROPERTY(EditAnywhere, Category = Brush2, meta = (DisplayName = "Brush Type"))
	UPROPERTY()
	EMeshGroupPaintBrushType PrimaryBrushType = EMeshGroupPaintBrushType::Paint;

	UPROPERTY(EditAnywhere, Category = ActionType, meta = (DisplayName = "Action"))
	EMeshGroupPaintInteractionType SubToolType = EMeshGroupPaintInteractionType::Brush;

	/** Relative size of brush */
	UPROPERTY(EditAnywhere, Category = ActionType, meta = (DisplayName = "Brush Size", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "10.0", 
		HideEditConditionToggle, EditConditionHides, EditCondition = "SubToolType != EMeshGroupPaintInteractionType::PolyLasso"))
	float BrushSize = 0.25f;

	/** When Volumetric, all faces inside the brush sphere are selected, otherwise only connected faces are selected */
	UPROPERTY(EditAnywhere, Category = ActionType, meta = (DisplayName = "Brush Area Mode",
		HideEditConditionToggle, EditConditionHides, EditCondition = "SubToolType != EMeshGroupPaintInteractionType::PolyLasso"))
	EMeshGroupPaintBrushAreaType BrushAreaMode = EMeshGroupPaintBrushAreaType::Connected;

	/** Allow the Brush to hit the back-side of the mesh */
	UPROPERTY(EditAnywhere, Category = ActionType, meta = (DisplayName = "Hit Back Faces",
		HideEditConditionToggle, EditConditionHides, EditCondition = "SubToolType != EMeshGroupPaintInteractionType::PolyLasso"))
	bool bHitBackFaces = true;

	/** The group that will be assigned to triangles */
	UPROPERTY(EditAnywhere, Category = ActionType, meta = (UIMin = 0, ClampMin = 0) )
	int32 SetGroup = 1;

	/** If true, only triangles with no group assigned will be painted */
	UPROPERTY(EditAnywhere, Category = ActionType)
	bool bOnlySetUngrouped = false;

	/** Group to set as Erased value */
	UPROPERTY(EditAnywhere, Category = ActionType, meta = (UIMin = 0, ClampMin = 0))
	int32 EraseGroup = 0;

	/** When enabled, only the current group configured in the Paint brush is erased */
	UPROPERTY(EditAnywhere, Category = ActionType)
	bool bOnlyEraseCurrent = false;

	/** The Region affected by the current operation will be bounded by edge angles larger than this threshold */
	UPROPERTY(EditAnywhere, Category = Filters, meta = (UIMin = "0.0", UIMax = "180.0", EditCondition = "SubToolType != EMeshGroupPaintInteractionType::PolyLasso && BrushAreaMode == EMeshGroupPaintBrushAreaType::Connected"))
	float AngleThreshold = 180.0f;

	/** The Region affected by the current operation will be bounded by UV borders/seams */
	UPROPERTY(EditAnywhere, Category = Filters, meta = (EditCondition = "SubToolType != EMeshGroupPaintInteractionType::PolyLasso && BrushAreaMode == EMeshGroupPaintBrushAreaType::Connected"))
	bool bUVSeams = false;

	/** The Region affected by the current operation will be bounded by Hard Normal edges/seams */
	UPROPERTY(EditAnywhere, Category = Filters, meta = (EditCondition = "SubToolType != EMeshGroupPaintInteractionType::PolyLasso && BrushAreaMode == EMeshGroupPaintBrushAreaType::Connected"))
	bool bNormalSeams = false;

	/** Control which triangles can be affected by the current operation based on visibility. Applied after all other filters. */
	UPROPERTY(EditAnywhere, Category = Filters)
	EMeshGroupPaintVisibilityType VisibilityFilter = EMeshGroupPaintVisibilityType::None;


	/** Number of vertices in a triangle the Lasso must hit to be counted as "inside" */
	UPROPERTY(EditAnywhere, Category = Filters, AdvancedDisplay, meta = (UIMin = 1, UIMax = 3, EditCondition = "SubToolType == EMeshGroupPaintInteractionType::PolyLasso"))
	int MinTriVertCount = 1;
};





UENUM()
enum class EMeshGroupPaintToolActions
{
	NoAction,

	ClearFrozen,
	FreezeCurrent,
	FreezeOthers,

	GrowCurrent,
	ShrinkCurrent,
	ClearCurrent,
	FloodFillCurrent,
	ClearAll
};


UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshGroupPaintToolActionPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UMeshGroupPaintTool> ParentTool;

	void Initialize(UMeshGroupPaintTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EMeshGroupPaintToolActions Action);
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshGroupPaintToolFreezeActions : public UMeshGroupPaintToolActionPropertySet
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, Category = Freezing, meta = (DisplayPriority = 1))
	void UnfreezeAll()
	{
		PostAction(EMeshGroupPaintToolActions::ClearFrozen);
	}

	UFUNCTION(CallInEditor, Category = Freezing, meta = (DisplayPriority = 2))
	void FreezeCurrent()
	{
		PostAction(EMeshGroupPaintToolActions::FreezeCurrent);
	}

	UFUNCTION(CallInEditor, Category = Freezing, meta = (DisplayPriority = 3))
	void FreezeOthers()
	{
		PostAction(EMeshGroupPaintToolActions::FreezeOthers);
	}

	UFUNCTION(CallInEditor, Category = Operations, meta = (DisplayPriority = 10))
	void ClearAll()
	{
		PostAction(EMeshGroupPaintToolActions::ClearAll);
	}

	UFUNCTION(CallInEditor, Category = Operations, meta = (DisplayPriority = 11))
	void ClearCurrent()
	{
		PostAction(EMeshGroupPaintToolActions::ClearCurrent);
	}

	UFUNCTION(CallInEditor, Category = Operations, meta = (DisplayPriority = 12))
	void FloodFillCurrent()
	{
		PostAction(EMeshGroupPaintToolActions::FloodFillCurrent);
	}


	UFUNCTION(CallInEditor, Category = Operations, meta = (DisplayPriority = 20))
	void GrowCurrent()
	{
		PostAction(EMeshGroupPaintToolActions::GrowCurrent);
	}

	UFUNCTION(CallInEditor, Category = Operations, meta = (DisplayPriority = 21))
	void ShrinkCurrent()
	{
		PostAction(EMeshGroupPaintToolActions::ShrinkCurrent);
	}


};




/**
 * Mesh Element Paint Tool Class
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshGroupPaintTool : public UMeshSculptToolBase
{
	GENERATED_BODY()

public:
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }

	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	bool IsInBrushSubMode() const;

	virtual void CommitResult(UBaseDynamicMeshComponent* Component, bool bModifiedTopology) override;


public:

	UPROPERTY()
	TObjectPtr<UPolygroupLayersProperties> PolygroupLayerProperties;

	/** Filters on paint brush */
	UPROPERTY()
	TObjectPtr<UGroupPaintBrushFilterProperties> FilterProperties;


private:
	// This will be of type UGroupPaintBrushOpProps, we keep a ref so we can change active group ID on pick
	UPROPERTY()
	TObjectPtr<UGroupPaintBrushOpProps> PaintBrushOpProperties;

	UPROPERTY()
	TObjectPtr<UGroupEraseBrushOpProps> EraseBrushOpProperties;

public:
	void AllocateNewGroupAndSetAsCurrentAction();
	void GrowCurrentGroupAction();
	void ShrinkCurrentGroupAction();
	void ClearCurrentGroupAction();
	void FloodFillCurrentGroupAction();
	void ClearAllGroupsAction();

	void SetTrianglesToGroupID(const TSet<int32>& Triangles, int32 ToGroupID, bool bIsErase);

	bool HaveVisibilityFilter() const;
	void ApplyVisibilityFilter(const TArray<int32>& Triangles, TArray<int32>& VisibleTriangles);
	void ApplyVisibilityFilter(TSet<int32>& Triangles, TArray<int32>& ROIBuffer, TArray<int32>& OutputBuffer);

	// we override these so we can update the separate BrushSize property added for this tool
	virtual void IncreaseBrushRadiusAction();
	virtual void DecreaseBrushRadiusAction();
	virtual void IncreaseBrushRadiusSmallStepAction();
	virtual void DecreaseBrushRadiusSmallStepAction();

protected:
	// UMeshSculptToolBase API
	virtual UBaseDynamicMeshComponent* GetSculptMeshComponent() { return DynamicMeshComponent; }
	virtual FDynamicMesh3* GetBaseMesh() { check(false); return nullptr; }
	virtual const FDynamicMesh3* GetBaseMesh() const { check(false); return nullptr; }

	virtual int32 FindHitSculptMeshTriangle(const FRay3d& LocalRay) override;
	virtual int32 FindHitTargetMeshTriangle(const FRay3d& LocalRay) override;

	virtual void OnBeginStroke(const FRay& WorldRay) override;
	virtual void OnEndStroke() override;

	virtual TUniquePtr<FMeshSculptBrushOp>& GetActiveBrushOp();
	// end UMeshSculptToolBase API



	//
	// Action support
	//

public:
	virtual void RequestAction(EMeshGroupPaintToolActions ActionType);

	UPROPERTY()
	TObjectPtr<UMeshGroupPaintToolFreezeActions> FreezeActions;

protected:
	bool bHavePendingAction = false;
	EMeshGroupPaintToolActions PendingAction;
	virtual void ApplyAction(EMeshGroupPaintToolActions ActionType);



	//
	// Marquee Support
	//
public:
	UPROPERTY()
	TObjectPtr<UPolyLassoMarqueeMechanic> PolyLassoMechanic;

protected:
	void OnPolyLassoFinished(const FCameraPolyLasso& Lasso, bool bCanceled);


	//
	// Internals
	//

protected:

	UPROPERTY()
	TObjectPtr<AInternalToolFrameworkActor> PreviewMeshActor = nullptr;

	UPROPERTY()
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;

	UPROPERTY()
	TObjectPtr<UMeshElementsVisualizer> MeshElementsDisplay;

	// realtime visualization
	void OnDynamicMeshComponentChanged(UDynamicMeshComponent* Component, const FMeshVertexChange* Change, bool bRevert);
	FDelegateHandle OnDynamicMeshComponentChangedHandle;

	TUniquePtr<UE::Geometry::FPolygroupSet> ActiveGroupSet;
	void OnSelectedGroupLayerChanged();
	void UpdateActiveGroupLayer();

	void UpdateSubToolType(EMeshGroupPaintInteractionType NewType);

	void UpdateBrushType(EMeshGroupPaintBrushType BrushType);

	TSet<int32> AccumulatedTriangleROI;
	bool bUndoUpdatePending = false;
	TArray<int> NormalsBuffer;
	void WaitForPendingUndoRedo();

	TArray<int> TempROIBuffer;
	TArray<int> VertexROI;
	TArray<bool> VisibilityFilterBuffer;
	TSet<int> VertexSetBuffer;
	TSet<int> TriangleROI;
	void UpdateROI(const FSculptBrushStamp& CurrentStamp);

	EMeshGroupPaintBrushType PendingStampType = EMeshGroupPaintBrushType::Paint;

	bool UpdateStampPosition(const FRay& WorldRay);
	bool ApplyStamp();

	UE::Geometry::FDynamicMeshOctree3 Octree;

	bool UpdateBrushPosition(const FRay& WorldRay);

	bool GetInEraseStroke()
	{
		// Re-use the smoothing stroke key (shift) for erase stroke in the group paint tool
		return GetInSmoothingStroke();
	}


	bool bPendingPickGroup = false;
	bool bPendingToggleFreezeGroup = false;


	TArray<int32> ROITriangleBuffer;
	TArray<int32> ROIGroupBuffer;
	bool SyncMeshWithGroupBuffer(FDynamicMesh3* Mesh);

	TUniquePtr<FDynamicMeshGroupEditBuilder> ActiveGroupEditBuilder;
	void BeginChange();
	void EndChange();

	TArray<int32> FrozenGroups;
	void ToggleFrozenGroup(int32 GroupID);
	void FreezeOtherGroups(int32 GroupID);
	void ClearAllFrozenGroups();
	void EmitFrozenGroupsChange(const TArray<int32>& FromGroups, const TArray<int32>& ToGroups, const FText& ChangeText);

	FColor GetColorForGroup(int32 GroupID);

	TArray<FVector3d> TriNormals;
	TArray<int32> UVSeamEdges;
	TArray<int32> NormalSeamEdges;
	void PrecomputeFilterData();


protected:
	virtual bool ShowWorkPlane() const override { return false; }


};



