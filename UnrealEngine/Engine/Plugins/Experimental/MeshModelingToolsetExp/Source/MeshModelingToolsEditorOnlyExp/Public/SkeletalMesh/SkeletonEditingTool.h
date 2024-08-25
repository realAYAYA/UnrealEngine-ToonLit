// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshBrushTool.h"
#include "GroupTopology.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"

#include "SkeletalMeshNotifier.h"
#include "SkeletonModifier.h"
#include "SkeletalMesh/SkeletalMeshEditionInterface.h"
#include "Engine/World.h"
#include "Changes/ValueWatcher.h"
#include "Mechanics/RectangleMarqueeMechanic.h"
#include "Misc/EnumClassFlags.h"
#include "Selection/MeshTopologySelectionMechanic.h"

#include "SkeletonEditingTool.generated.h"

class USingleClickInputBehavior;
class UGizmoViewContext;
class UTransformGizmo;
class USkeletonEditingTool;
class USkeletonTransformProxy;
class USkeletalMeshGizmoContextObjectBase;
class USkeletalMeshGizmoWrapperBase;
class UPolygonSelectionMechanic;

namespace SkeletonEditingTool
{

/**
 * A wrapper change class that stores a reference skeleton and the bones' indexes trackers to be used for undo/redo.
 */
class MESHMODELINGTOOLSEDITORONLYEXP_API FRefSkeletonChange : public FToolCommandChange
{
public:
	FRefSkeletonChange(const USkeletonEditingTool* InTool);

	virtual FString ToString() const override
	{
		return FString(TEXT("Edit Skeleton"));
	}

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;

	void StoreSkeleton(const USkeletonEditingTool* InTool);

private:
	FReferenceSkeleton PreChangeSkeleton;
	TArray<int32> PreBoneTracker;
	FReferenceSkeleton PostChangeSkeleton;
	TArray<int32> PostBoneTracker;
};
	
}

/**
 * USkeletonEditingToolBuilder
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkeletonEditingToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:
	// UInteractiveToolBuilder overrides
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
	
protected:
	// UInteractiveToolWithToolTargetsBuilder overrides
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};

/**
 * EEditingOperation represents the current tool's operation 
 */

UENUM()
enum class EEditingOperation : uint8
{
	Select,		// Selecting bones in the viewport.
	Create,		// Creating bones in the viewport.
	Remove,		// Removing current selection.
	Transform,	// Transforming bones in the viewport or thru the details panel.
	Parent,		// Parenting bones in the viewport.
	Rename,		// Renaming bones thru the details panel.
	Mirror		// Mirroring bones thru the details panel.
};

/**
 * USkeletonEditingTool is a tool to edit a the ReferenceSkeleton of a SkeletalMesh (target)
 * Changed are actually commit to the SkeletalMesh and it's mesh description on Accept.
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkeletonEditingTool :
	public USingleSelectionTool,
	public IClickDragBehaviorTarget,
	public ISkeletalMeshEditingInterface
{
	GENERATED_BODY()

public:

	void Init(const FToolBuilderState& InSceneState);
	
	// UInteractiveTool overrides
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// ICLickDragBehaviorTarget overrides
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& InPressPos) override;
	
	virtual void OnClickPress(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& InReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	
	virtual void OnTick(float DeltaTime) override;

	// IInteractiveToolCameraFocusAPI overrides
	virtual FBox GetWorldSpaceFocusBox() override;

	// Modifier functions
	void MirrorBones();
	void RenameBones();
	void MoveBones();
	void OrientBones();
	void RemoveBones();
	void UnParentBones();
	void SnapBoneToComponentSelection(const bool bCreate);
	
	// ISkeletalMeshEditionInterface overrides
	virtual TArray<FName> GetSelectedBones() const override;

	const TArray<FName>& GetSelection() const;

	const FTransform& GetTransform(const FName InBoneName, const bool bWorld) const;
	void SetTransforms(const TArray<FName>& InBones, const TArray<FTransform>& InTransforms, const bool bWorld) const;

	// IModifierToggleBehaviorTarget overrides
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;

	// ISkeletalMeshEditionInterface overrides
	virtual TWeakObjectPtr<USkeletonModifier> GetModifier() const override;

	EEditingOperation GetOperation() const;
	void SetOperation(const EEditingOperation InOperation, const bool bUpdateGizmo = true);

	bool HasSelectedComponent() const;
	
protected:

	// UInteractiveTool overrides
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	// ISkeletalMeshEditionInterface overrides
	virtual void HandleSkeletalMeshModified(const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType) override;

	// Modifier functions
	void CreateNewBone();
	void ParentBones(const FName& InParentName);

public:
	
	UPROPERTY()
	TObjectPtr<USkeletonEditingProperties> Properties;

	UPROPERTY()
	TObjectPtr<UProjectionProperties> ProjectionProperties;
	
	UPROPERTY()
	TObjectPtr<UMirroringProperties> MirroringProperties;

	UPROPERTY()
	TObjectPtr<UOrientingProperties> OrientingProperties;

	UPROPERTY()
	TObjectPtr<UPolygonSelectionMechanic> SelectionMechanic;
	
protected:
	
	UPROPERTY()
	TObjectPtr<USkeletonModifier> Modifier = nullptr;
	
	UPROPERTY()
	TObjectPtr<USingleClickInputBehavior> LeftClickBehavior = nullptr;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UGizmoViewContext> ViewContext = nullptr;

	UPROPERTY()
	EEditingOperation Operation = EEditingOperation::Select;

	// gizmo
	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshGizmoContextObjectBase> GizmoContext = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshGizmoWrapperBase> GizmoWrapper = nullptr;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshEditorContextObjectBase> EditorContext = nullptr;

	void UpdateGizmo() const;
	
	// ref skeleton transactions
	void BeginChange();
	void EndChange();
	void CancelChange();
	TUniquePtr<SkeletonEditingTool::FRefSkeletonChange> ActiveChange;

	friend class SkeletonEditingTool::FRefSkeletonChange;
	
private:
	TArray<int32> GetSelectedBoneIndexes() const;
	
	enum class EBoneSelectionMode : uint8
	{
		Single				= 0,
		Additive			= 1 << 0,
		Toggle				= 1 << 1
	};
	FRIEND_ENUM_CLASS_FLAGS(USkeletonEditingTool::EBoneSelectionMode)
	

	// flags used to identify behavior modifier keys/buttons
	static constexpr int AddToSelectionModifier = 1;
	static constexpr int ToggleSelectionModifier = 2;
	EBoneSelectionMode SelectionMode = EBoneSelectionMode::Single;

	TArray<FName> Selection;
	
	void SelectBone(const FName& InBoneName);
	FName GetCurrentBone() const;

	void NormalizeSelection();

	// setup
	void SetupModifier(USkeletalMesh* InSkeletalMesh);
	void SetupPreviewMesh();
	void SetupProperties();
	void SetupBehaviors();
	void SetupGizmo(USkeletalMeshComponent* InComponent);
	void SetupWatchers();
	void SetupComponentsSelection();
	
	// actions
	void RegisterCreateAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterDeleteAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterSelectAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterParentAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterUnParentAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterCopyAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterPasteAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterDuplicateAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterSelectComponentsAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterSelectionFilterCyclingAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);
	void RegisterSnapAction(FInteractiveToolActionSet& InOutActionSet, const int32 InActionId);

	TArray<int32> GetSelectedComponents() const;

	FTransform ComputeTransformFromComponents(const TArray<int32>& InIDs) const;
	
	TValueWatcher<TArray<FName>> SelectionWatcher;
	TValueWatcher<EToolContextCoordinateSystem> CoordinateSystemWatcher; 
	
	int32 ParentIndex = INDEX_NONE;

	TFunction<void()> PendingFunction;

	TUniquePtr<UE::Geometry::FTriangleGroupTopology> Topology = nullptr;
};

ENUM_CLASS_FLAGS(USkeletonEditingTool::EBoneSelectionMode);

/**
 * USkeletonEditingProperties
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkeletonEditingProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	void Initialize(USkeletonEditingTool* ParentToolIn);

	UPROPERTY(EditAnywhere, Category = "Details")
	FName Name;

	UPROPERTY()
	FTransform Transform;
	
	UPROPERTY(EditAnywhere, Category = "Details")
	bool bUpdateChildren = false;

	UPROPERTY(EditAnywhere, Category = "Viewport Axis Settings",  meta = (DisplayPriority = 10, ClampMin = "0.0", UIMin = "0.0"))
	float AxisLength = 1.f;

	UPROPERTY(EditAnywhere, Category = "Viewport Axis Settings",  meta = (DisplayPriority = 10, ClampMin = "0.0", UIMin = "0.0"))
	float AxisThickness = 0.f;

	UPROPERTY()
	bool bEnableComponentSelection = false;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif
	
	TWeakObjectPtr<USkeletonEditingTool> ParentTool = nullptr;
};

/**
 * EProjectionType
 */

UENUM()
enum class EProjectionType : uint8
{
	CameraPlane,	// The camera plane is used as the projection plane 
	OnMesh,			// The mesh surface is used for projection
	WithinMesh,		// The mesh volume is used for projection
};

/**
 * UProjectionProperties
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UProjectionProperties: public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	void Initialize(USkeletonEditingTool* ParentToolIn, TObjectPtr<UPreviewMesh> PreviewMesh);

	void UpdatePlane(const UGizmoViewContext& InViewContext, const FVector& InOrigin);
	bool GetProjectionPoint(const FInputDeviceRay& InRay, FVector& OutHitPoint) const;
	
	UPROPERTY(EditAnywhere, Category = "Project")
	EProjectionType ProjectionType = EProjectionType::WithinMesh;

	FViewCameraState CameraState;

	TWeakObjectPtr<USkeletonEditingTool> ParentTool = nullptr;
	
private:
	TWeakObjectPtr<UPreviewMesh> PreviewMesh = nullptr;
	
	UPROPERTY()
	FVector PlaneOrigin = FVector::ZeroVector;
	
	UPROPERTY()
	FVector PlaneNormal =  FVector::ZAxisVector;
};

/**
 * UMirroringProperties
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMirroringProperties: public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	void Initialize(USkeletonEditingTool* ParentToolIn);

	void MirrorBones();

	UPROPERTY(EditAnywhere, Category = "Mirror")
	FMirrorOptions Options;

	TWeakObjectPtr<USkeletonEditingTool> ParentTool = nullptr;
};

/**
 * UOrientingProperties
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UOrientingProperties: public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	void Initialize(USkeletonEditingTool* ParentToolIn);

	void OrientBones();

	UPROPERTY(EditAnywhere, Category = "Orient")
	bool bAutoOrient = false;
	
	UPROPERTY(EditAnywhere, Category = "Orient")
	FOrientOptions Options;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif
	
	TWeakObjectPtr<USkeletonEditingTool> ParentTool = nullptr;
};