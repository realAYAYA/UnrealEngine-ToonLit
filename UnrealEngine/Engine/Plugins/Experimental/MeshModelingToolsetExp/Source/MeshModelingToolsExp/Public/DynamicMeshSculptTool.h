// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "SubRegionRemesher.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "Components/OctreeDynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshOctree3.h"
#include "DynamicMesh/MeshNormals.h"
#include "BaseTools/BaseBrushTool.h"
#include "ToolDataVisualizer.h"
#include "Changes/ValueWatcher.h"
#include "BaseGizmos/BrushStampIndicator.h"
#include "Properties/MeshMaterialProperties.h"
#include "Properties/RemeshProperties.h"
#include "TransformTypes.h"
#include "Sculpting/MeshSculptToolBase.h"
#include "Async/Async.h"
#include "Util/UniqueIndexSet.h"
#include "DynamicMeshSculptTool.generated.h"

class UCombinedTransformGizmo;
class UTransformProxy;
class UMaterialInstanceDynamic;

class FMeshVertexChangeBuilder;
class UPreviewMesh;
PREDECLARE_GEOMETRY(class FDynamicMeshChangeTracker);
PREDECLARE_GEOMETRY(class FSubRegionRemesher);
PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);
class FPersistentStampRemesher;


/** Mesh Sculpting Brush Types */
UENUM()
enum class EDynamicMeshSculptBrushType : uint8
{
	/** Move vertices parallel to the view plane  */
	Move UMETA(DisplayName = "Move"),

	/** Grab Brush, fall-off alters the influence of the grab */
	PullKelvin UMETA(DisplayName = "Kelvin Grab"),

	/** Grab Brush that may generate cusps, fall-off alters the influence of the grab */
	PullSharpKelvin UMETA(DisplayName = "Sharp Kelvin Grab"),

	/** Smooth mesh vertices  */
	Smooth UMETA(DisplayName = "Smooth"),

	/** Displace vertices along the average surface normal (Ctrl to invert) */
	Offset UMETA(DisplayName = "Sculpt (Normal)"),

	/** Displace vertices towards the camera viewpoint (Ctrl to invert) */
	SculptView UMETA(DisplayName = "Sculpt (Viewpoint)"),

	/** Displaces vertices along the average surface normal to a maximum height based on the brush size (Ctrl to invert) */
	SculptMax UMETA(DisplayName = "Sculpt Max"),

	/** Displace vertices along their vertex normals */
	Inflate UMETA(DisplayName = "Inflate"),

	/** Scale Brush will inflate or pinch radially from the center of the brush */
	ScaleKelvin UMETA(DisplayName = "Kelvin Scale"),

	/** Move vertices towards the center of the brush (Ctrl to push away)*/
	Pinch UMETA(DisplayName = "Pinch"),

	/** Twist Brush moves vertices in the plane perpendicular to the local mesh normal */
	TwistKelvin UMETA(DisplayName = "Kelvin Twist"),

	/** Move vertices towards the average plane of the brush stamp region */
	Flatten UMETA(DisplayName = "Flatten"),

	/** Move vertices towards a plane defined by the initial brush position  */
	Plane UMETA(DisplayName = "Plane (Normal)"),

	/** Move vertices towards a view-facing plane defined at the initial brush position */
	PlaneViewAligned UMETA(DisplayName = "Plane (Viewpoint)"),

	/** Move vertices towards a fixed plane in world space, positioned with a 3D gizmo */
	FixedPlane UMETA(DisplayName = "FixedPlane"),

	/** Remesh the brushed region but do not otherwise deform it */
	Resample UMETA(DisplayName = "Resample"),

	LastValue UMETA(Hidden)

};

/**
 * Tool Builder
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UDynamicMeshSculptToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	bool bEnableRemeshing;

	UDynamicMeshSculptToolBuilder()
	{
		bEnableRemeshing = false;
	}

	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};





UCLASS()
class MESHMODELINGTOOLSEXP_API UDynamicMeshBrushProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayPriority = 1))
	FBrushToolRadius BrushSize;

	/** Amount of falloff to apply (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Falloff", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = 3))
	float BrushFalloffAmount = 0.5f;

	/** Depth of Brush into surface along view ray or surface normal, depending on the Active Brush Type */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (UIMin = "-0.5", UIMax = "0.5", ClampMin = "-1.0", ClampMax = "1.0", DisplayPriority = 5))
	float Depth = 0;

	/** Allow the Brush to hit the back-side of the mesh */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayPriority = 6))
	bool bHitBackFaces = true;
};





UCLASS()
class MESHMODELINGTOOLSEXP_API UDynamicMeshBrushSculptProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/* This is a dupe of the bool in the tool class.  I needed it here so it could be checked as an EditCondition */
	UPROPERTY(meta = (TransientToolProperty))
	bool bIsRemeshingEnabled = false;

	/** Primary Brush Mode */
	UPROPERTY(EditAnywhere, Category = Sculpting, meta = (DisplayName = "Brush Type"))
	EDynamicMeshSculptBrushType PrimaryBrushType = EDynamicMeshSculptBrushType::Move;

	/** Strength of the Primary Brush */
	UPROPERTY(EditAnywhere, Category = Sculpting, meta = (DisplayName = "Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "PrimaryBrushType != EDynamicMeshSculptBrushType::Pull"))
	float PrimaryBrushSpeed = 0.5;

	/** If true, try to preserve the shape of the UV/3D mapping. This will limit Smoothing and Remeshing in some cases. */
	UPROPERTY(EditAnywhere, Category = Sculpting)
	bool bPreserveUVFlow = false;

	/** When Freeze Target is toggled on, the Brush Target Surface will be Frozen in its current state, until toggled off. Brush strokes will be applied relative to the Target Surface, for applicable Brushes */
	UPROPERTY(EditAnywhere, Category = Sculpting, meta = (EditCondition = "PrimaryBrushType == EDynamicMeshSculptBrushType::Sculpt || PrimaryBrushType == EDynamicMeshSculptBrushType::SculptMax || PrimaryBrushType == EDynamicMeshSculptBrushType::SculptView || PrimaryBrushType == EDynamicMeshSculptBrushType::Pinch || PrimaryBrushType == EDynamicMeshSculptBrushType::Resample" ))
	bool bFreezeTarget = false;

	/** Strength of Shift-to-Smooth Brushing and Smoothing Brush */
	UPROPERTY(EditAnywhere, Category = Smoothing, meta = (DisplayName = "Smoothing Strength", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float SmoothBrushSpeed = 0.25;

	/** If enabled, Remeshing is limited during Smoothing to avoid wiping out higher-density triangle areas */
	UPROPERTY(EditAnywhere, Category = Smoothing, meta = (DisplayName = "Preserve Tri Density", EditConditionHides, HideEditConditionToggle, EditCondition = "bIsRemeshingEnabled"))
	bool bDetailPreservingSmooth = true;
};


UCLASS()
class MESHMODELINGTOOLSEXP_API UDynamicSculptToolActions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<UDynamicMeshSculptTool> ParentTool;

	void Initialize(UDynamicMeshSculptTool* ParentToolIn) { ParentTool = ParentToolIn; }

	UFUNCTION(CallInEditor, Category = MeshEdits)
	void DiscardAttributes();
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UBrushRemeshProperties : public URemeshProperties
{
	GENERATED_BODY()

public:
	/** Toggle remeshing on/off */
	UPROPERTY(EditAnywhere, Category = Remeshing, meta = (DisplayPriority = 1))
	bool bEnableRemeshing = true;

	// Note that if you change range here, you must also update UDynamicMeshSculptTool::ConfigureRemesher!
	/** Desired size of triangles after Remeshing, relative to average initial triangle size. Larger value results in larger triangles. */
	UPROPERTY(EditAnywhere, Category = Remeshing, meta = (DisplayName = "Relative Tri Size", UIMin = "-5", UIMax = "5", ClampMin = "-5", ClampMax = "5", DisplayPriority = 2))
	int TriangleSize = 0;

	/** Control the amount of simplification during sculpting. Higher values will avoid wiping out fine details on the mesh. */
	UPROPERTY(EditAnywhere, Category = Remeshing, meta = (UIMin = "0", UIMax = "5", ClampMin = "0", ClampMax = "5", DisplayPriority = 3))
	int PreserveDetail = 0;

	UPROPERTY(EditAnywhere, Category = Remeshing, AdvancedDisplay)
	int Iterations = 5;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UFixedPlaneBrushProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY( meta = (TransientToolProperty) )
	bool bPropertySetEnabled = true;

	/** Toggle whether Work Plane Positioning Gizmo is visible */
	UPROPERTY(EditAnywhere, Category = TargetPlane, meta = (HideEditConditionToggle, EditCondition = "bPropertySetEnabled == true"))
	bool bShowGizmo = true;

	UPROPERTY(EditAnywhere, Category = TargetPlane, AdvancedDisplay, meta = (HideEditConditionToggle, EditCondition = "bPropertySetEnabled == true"))
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = TargetPlane, AdvancedDisplay, meta = (HideEditConditionToggle, EditCondition = "bPropertySetEnabled == true"))
	FQuat Rotation = FQuat::Identity;

	// Recenter the gizmo around the target position (without changing work plane), if it is "too far" (> 10 meters + max bounds dim) from that position currently
	void RecenterGizmoIfFar(FVector CenterPosition, double BoundsMaxDim, double TooFarDistance = 1000)
	{
		double DistanceTolSq = (BoundsMaxDim + TooFarDistance) * (BoundsMaxDim + TooFarDistance);
		if (FVector::DistSquared(CenterPosition, Position) > DistanceTolSq)
		{
			FVector Normal = Rotation.GetAxisZ();
			Position = CenterPosition - (CenterPosition - Position).ProjectOnToNormal(Normal);
		}
	}
};




/**
 * Dynamic Mesh Sculpt Tool Class
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UDynamicMeshSculptTool : public UMeshSurfacePointTool
{
	GENERATED_BODY()

public:
	using FFrame3d = UE::Geometry::FFrame3d;

	UDynamicMeshSculptTool();

	virtual void SetWorld(UWorld* World);
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;

	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;

	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

	virtual void SetEnableRemeshing(bool bEnable) { bEnableRemeshing = bEnable; }
	virtual bool GetEnableRemeshing() const { return bEnableRemeshing; }

	virtual void DiscardAttributes();

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IInteractiveToolCameraFocusAPI override to focus on brush w/ 'F' 
	virtual FBox GetWorldSpaceFocusBox() override;

public:
	/** Properties that control brush size/etc*/
	UPROPERTY()
	TObjectPtr<UDynamicMeshBrushProperties> BrushProperties;

	/** Properties that control sculpting*/
	UPROPERTY()
	TObjectPtr<UDynamicMeshBrushSculptProperties> SculptProperties;

	UPROPERTY()
	TObjectPtr<USculptMaxBrushProperties> SculptMaxBrushProperties;
	
	UPROPERTY()
	TObjectPtr<UKelvinBrushProperties> KelvinBrushProperties;

	/** Properties that control dynamic remeshing */
	UPROPERTY()
	TObjectPtr<UBrushRemeshProperties> RemeshProperties;

	UPROPERTY()
	TObjectPtr<UFixedPlaneBrushProperties> GizmoProperties;

	UPROPERTY()
	TObjectPtr<UMeshEditingViewProperties> ViewProperties;

	UPROPERTY()
	TObjectPtr<UDynamicSculptToolActions> SculptToolActions;

public:
	virtual void IncreaseBrushRadiusAction();
	virtual void DecreaseBrushRadiusAction();
	virtual void IncreaseBrushRadiusSmallStepAction();
	virtual void DecreaseBrushRadiusSmallStepAction();

	virtual void IncreaseBrushSpeedAction();
	virtual void DecreaseBrushSpeedAction();

	virtual void NextHistoryBrushModeAction();
	virtual void PreviousHistoryBrushModeAction();

private:
	UWorld* TargetWorld;		// required to spawn UPreviewMesh/etc
	FViewCameraState CameraState;

	UPROPERTY()
	TObjectPtr<UBrushStampIndicator> BrushIndicator;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BrushIndicatorMaterial;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> BrushIndicatorMesh;

	UPROPERTY()
	TObjectPtr<UOctreeDynamicMeshComponent> DynamicMeshComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActiveOverrideMaterial;

	UE::Geometry::FTransformSRT3d InitialTargetTransform;
	UE::Geometry::FTransformSRT3d CurTargetTransform;

	// realtime visualization
	void OnDynamicMeshComponentChanged();
	FDelegateHandle OnDynamicMeshComponentChangedHandle;

	TValueWatcher<bool> ShowWireframeWatcher;
	TValueWatcher<EMeshEditingMaterialModes> MaterialModeWatcher;
	TValueWatcher<TWeakObjectPtr<UMaterialInterface>> CustomMaterialWatcher;
	TValueWatcher<bool> FlatShadingWatcher;
	TValueWatcher<FLinearColor> ColorWatcher;
	TValueWatcher<FLinearColor> TransparentColorWatcher;
	TValueWatcher<double> OpacityWatcher;
	TValueWatcher<bool> TwoSidedWatcher;
	TValueWatcher<UTexture2D*> ImageWatcher;
	TValueWatcher<EDynamicMeshSculptBrushType> BrushTypeWatcher;
	TValueWatcher<FVector> GizmoPositionWatcher;
	TValueWatcher<FQuat> GizmoRotationWatcher;
	void UpdateMaterialMode(EMeshEditingMaterialModes NewMode);
	void UpdateFlatShadingSetting(bool bNewValue);
	void UpdateColorSetting(FLinearColor NewColor);
	void UpdateOpacitySetting(double Opacity);
	void UpdateTwoSidedSetting(bool bOn);
	void UpdateCustomMaterial(TWeakObjectPtr<UMaterialInterface> NewMaterial);
	void UpdateImageSetting(UTexture2D* NewImage);
	void UpdateBrushType(EDynamicMeshSculptBrushType BrushType);
	void UpdateGizmoFromProperties();

	UE::Geometry::FInterval1d BrushRelativeSizeRange;
	double CurrentBrushRadius;
	void CalculateBrushRadius();

	bool bEnableRemeshing;
	double InitialEdgeLength;
	void ScheduleRemeshPass();
	void ConfigureRemesher(UE::Geometry::FSubRegionRemesher& Remesher);
	void InitializeRemesherROI(UE::Geometry::FSubRegionRemesher& Remesher);

	TSharedPtr<FPersistentStampRemesher> ActiveRemesher;
	void InitializeActiveRemesher();
	void PrecomputeRemesherROI();
	void RemeshROIPass_ActiveRemesher(bool bHasPrecomputedROI);


	bool bInDrag;

	UE::Geometry::FFrame3d ActiveDragPlane;
	FVector3d LastHitPosWorld;
	FVector3d BrushStartCenterWorld;
	FVector3d BrushStartNormalWorld;
	FVector3d LastBrushPosLocal;
	FVector3d LastBrushPosWorld;
	FVector3d LastBrushPosNormalWorld;
	FVector3d LastSmoothBrushPosLocal;
	int32 LastBrushTriangleID = -1;

	TArray<int> UpdateROITriBuffer;
	UE::Geometry::FUniqueIndexSet VertexROIBuilder;
	TArray<int> VertexROI;
	UE::Geometry::FUniqueIndexSet TriangleROIBuilder;
	TSet<int> TriangleROI;
	//TSet<int> TriangleROI;
	void UpdateROI(const FVector3d& BrushPos);

	bool bRemeshPending;
	bool bNormalUpdatePending;
	
	bool bTargetDirty;
	TFuture<void> PendingTargetUpdate;

	bool bSmoothing;
	bool bInvert;
	float ActivePressure = 1.0f;

	bool bHaveRemeshed;

	bool bStampPending;
	FRay PendingStampRay;
	int StampTimestamp = 0;
	EDynamicMeshSculptBrushType LastStampType = EDynamicMeshSculptBrushType::LastValue;
	EDynamicMeshSculptBrushType PendingStampType = LastStampType;
	void ApplyStamp(const FRay& WorldRay);

	FDynamicMesh3 BrushTargetMesh;
	UE::Geometry::FDynamicMeshAABBTree3 BrushTargetMeshSpatial;
	UE::Geometry::FMeshNormals BrushTargetNormals;
	bool bCachedFreezeTarget = false;
	void UpdateTarget();
	bool GetTargetMeshNearest(const FVector3d& Position, double SearchRadius, FVector3d& TargetPosOut, FVector3d& TargetNormalOut);

	int FindHitSculptMeshTriangle(const FRay3d& LocalRay);
	int FindHitTargetMeshTriangle(const FRay3d& LocalRay);
	bool IsHitTriangleBackFacing(int32 TriangleID, const FDynamicMesh3* QueryMesh);

	bool UpdateBrushPosition(const FRay& WorldRay);
	bool UpdateBrushPositionOnActivePlane(const FRay& WorldRay);
	bool UpdateBrushPositionOnTargetMesh(const FRay& WorldRay, bool bFallbackToViewPlane);
	bool UpdateBrushPositionOnSculptMesh(const FRay& WorldRay, bool bFallbackToViewPlane);
	void AlignBrushToView();

	bool ApplySmoothBrush(const FRay& WorldRay);
	bool ApplyMoveBrush(const FRay& WorldRay);
	bool ApplyOffsetBrush(const FRay& WorldRay, bool bUseViewDirection);
	bool ApplySculptMaxBrush(const FRay& WorldRay);
	bool ApplyPinchBrush(const FRay& WorldRay);
	bool ApplyInflateBrush(const FRay& WorldRay);
	bool ApplyPlaneBrush(const FRay& WorldRay);
	bool ApplyFixedPlaneBrush(const FRay& WorldRay);
	bool ApplyFlattenBrush(const FRay& WorldRay);
	bool ApplyResampleBrush(const FRay& WorldRay);
	bool ApplyPullKelvinBrush(const FRay& WorldRay);
	bool ApplyPullSharpKelvinBrush(const FRay& WorldRay);
	bool ApplyTwistKelvinBrush(const FRay& WorldRay);
	bool ApplyScaleKelvinBrush(const FRay& WorldRay);

	double SculptMaxFixedHeight = -1.0;

	double CalculateBrushFalloff(double Distance);
	TArray<FVector3d> ROIPositionBuffer;
	void SyncMeshWithPositionBuffer(FDynamicMesh3* Mesh);

	UE::Geometry::FFrame3d ActiveFixedBrushPlane;
	UE::Geometry::FFrame3d ComputeROIBrushPlane(const FVector3d& BrushCenter, bool bIgnoreDepth, bool bViewAligned);

	TArray<int> NormalsBuffer;
	TArray<bool> NormalsVertexFlags;
	void RecalculateNormals_PerVertex(const TSet<int32>& Triangles);
	void RecalculateNormals_Overlay(const TSet<int32>& Triangles);

	bool bHaveMeshBoundaries;
	bool bHaveUVSeams;
	bool bHaveNormalSeams;
	TSet<int32> RemeshRemovedTriangles;
	TSet<int32> RemeshFinalTriangleROI;
	void PrecomputeRemeshInfo();
	void RemeshROIPass();

	FMeshVertexChangeBuilder* ActiveVertexChange = nullptr;
	UE::Geometry::FDynamicMeshChangeTracker* ActiveMeshChange = nullptr;
	void BeginChange(bool bIsVertexChange);
	void EndChange();
	void SaveActiveROI();

	double EstimateIntialSafeTargetLength(const FDynamicMesh3& Mesh, int MinTargetTriCount);

	TArray<EDynamicMeshSculptBrushType> BrushTypeHistory;
	int BrushTypeHistoryIndex = 0;

	UPreviewMesh* MakeDefaultSphereMesh(UObject* Parent, UWorld* World, int Resolution = 32);

	//
	// support for gizmo in FixedPlane mode
	//

	// plane gizmo
	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> PlaneTransformGizmo;

	UPROPERTY()
	TObjectPtr<UTransformProxy> PlaneTransformProxy;

	void PlaneTransformChanged(UTransformProxy* Proxy, FTransform Transform);

	enum class EPendingWorkPlaneUpdate
	{
		NoUpdatePending,
		MoveToHitPositionNormal,
		MoveToHitPosition,
		MoveToHitPositionViewAligned
	};
	EPendingWorkPlaneUpdate PendingWorkPlaneUpdate;
	void SetFixedSculptPlaneFromWorldPos(const FVector& Position, const FVector& Normal, EPendingWorkPlaneUpdate UpdateType);
	void UpdateFixedSculptPlanePosition(const FVector& Position);
	void UpdateFixedSculptPlaneRotation(const FQuat& Rotation);
	void UpdateFixedPlaneGizmoVisibility(bool bVisible);
};



