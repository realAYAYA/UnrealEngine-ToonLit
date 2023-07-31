// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "Changes/DynamicMeshChangeTarget.h"
#include "InteractiveToolBuilder.h"
#include "Mechanics/ConstructionPlaneMechanic.h"
#include "MeshOpPreviewHelpers.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "Selection/SelectClickedAction.h"
#include "ToolContextInterfaces.h"

#include "MirrorTool.generated.h"

UCLASS()
class MESHMODELINGTOOLSEXP_API UMirrorToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};

UENUM()
enum class EMirrorSaveMode : uint8
{
	/**  Save the results in place of the original assets. */
	UpdateAssets = 0,

	/** Save the results as new assets. */
	CreateNewAssets = 1,
};

UENUM()
enum class EMirrorOperationMode : uint8
{
	/**  Append a mirrored version of the mesh to itself. */
	MirrorAndAppend = 0,

	/** Mirror the existing mesh. */
	MirrorExisting = 1,
};

UENUM()
enum class EMirrorCtrlClickBehavior : uint8
{
	/** Move the mirror plane to clicked location without adjusting its normal. */
	Reposition = 0,

	/** Move the mirror plane and adjust its normal according to click location. */
	RepositionAndReorient = 1,
};

/**
 * 
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMirrorToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	/** Mode of operation. */
	UPROPERTY(EditAnywhere, Category = Options)
	EMirrorOperationMode OperationMode = EMirrorOperationMode::MirrorAndAppend;

	/** Cut off everything on the back side of the mirror plane before mirroring. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bCropAlongMirrorPlaneFirst = true;

	/** Weld vertices that lie on the mirror plane. Vertices will not be welded if doing so would give an edge more than two faces, or if they are part of a face in the plane. */
	UPROPERTY(EditAnywhere, Category = Options, meta = (
		EditCondition = "OperationMode == EMirrorOperationMode::MirrorAndAppend", EditConditionHides))
	bool bWeldVerticesOnMirrorPlane = true;

	/** Distance (in unscaled mesh space) to allow a point to be from the plane and still consider it "on the mirror plane". */
	UPROPERTY(EditAnywhere, Category = Options, meta = (
		EditCondition = "OperationMode == EMirrorOperationMode::MirrorAndAppend && bWeldVerticesOnMirrorPlane", EditConditionHides,
		UIMin = 0, UIMax = 0.01, ClampMin = 0, ClampMax = 10))
	double PlaneTolerance = KINDA_SMALL_NUMBER;

	/** When welding, whether to allow bowtie vertices to be created, or to duplicate the vertex. */
	UPROPERTY(EditAnywhere, Category = Options, AdvancedDisplay, meta = (
		EditCondition = "bWeldVerticesOnMirrorPlane && OperationMode == EMirrorOperationMode::MirrorAndAppend", EditConditionHides))
	bool bAllowBowtieVertexCreation = false;

	/** What Ctrl + clicking does to the mirror plane. */
	UPROPERTY(EditAnywhere, Category = RepositionOptions)
	EMirrorCtrlClickBehavior CtrlClickBehavior = EMirrorCtrlClickBehavior::Reposition;

	/** If true the "Preset Mirror Directions" buttons only change the plane orientation, not location. */
	UPROPERTY(EditAnywhere, Category = RepositionOptions, AdvancedDisplay)
	bool bButtonsOnlyChangeOrientation = false;
	
	/** Whether to show the preview. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowPreview = true;

	/** How to save the result. */
	UPROPERTY(EditAnywhere, Category = ToolOutputOptions)
	EMirrorSaveMode SaveMode = EMirrorSaveMode::UpdateAssets;
};


UCLASS()
class MESHMODELINGTOOLSEXP_API UMirrorOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UMirrorTool> MirrorTool;

	/** Index of the component within MirrorTool->ComponentTargets that this factory creates an operator for. */
	int ComponentIndex;
};

UENUM()
enum class EMirrorToolAction
{
	NoAction,

	ShiftToCenter,

	Left,
	Right,
	Up,
	Down,
	Forward,
	Backward
};

UCLASS()
class MESHMODELINGTOOLSEXP_API UMirrorToolActionPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UMirrorTool> ParentTool;

	void Initialize(UMirrorTool* ParentToolIn) { ParentTool = ParentToolIn; }
	void PostAction(EMirrorToolAction Action);

	/** Move the mirror plane to center of bounding box without changing its normal. */
	UFUNCTION(CallInEditor, Category = RepositionPlane)
	void ShiftToCenter() { PostAction(EMirrorToolAction::ShiftToCenter); }

	/** Move the mirror plane and adjust its normal to mirror entire selection leftward. */
	UFUNCTION(CallInEditor, Category = PresetMirrorDirections, meta = (DisplayPriority = 1))
	void Left() { PostAction(EMirrorToolAction::Left); }

	/** Move the mirror plane and adjust its normal to mirror entire selection rightward. */
	UFUNCTION(CallInEditor, Category = PresetMirrorDirections, meta = (DisplayPriority = 2))
	void Right() { PostAction(EMirrorToolAction::Right); }

	/** Move the mirror plane and adjust its normal to mirror entire selection upward. */
	UFUNCTION(CallInEditor, Category = PresetMirrorDirections, meta = (DisplayPriority = 3))
	void Up() { PostAction(EMirrorToolAction::Up); }

	/** Move the mirror plane and adjust its normal to mirror entire selection downward. */
	UFUNCTION(CallInEditor, Category = PresetMirrorDirections, meta = (DisplayPriority = 4))
	void Down() { PostAction(EMirrorToolAction::Down); }

	/** Move the mirror plane and adjust its normal to mirror entire selection forward. */
	UFUNCTION(CallInEditor, Category = PresetMirrorDirections, meta = (DisplayPriority = 5))
	void Forward() { PostAction(EMirrorToolAction::Forward); }

	/** Move the mirror plane and adjust its normal to mirror entire selection backward. */
	UFUNCTION(CallInEditor, Category = PresetMirrorDirections, meta = (DisplayPriority = 6))
	void Backward() { PostAction(EMirrorToolAction::Backward); }
};

/** Tool for mirroring one or more meshes across a plane. */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMirrorTool : public UMultiSelectionMeshEditingTool, public IModifierToggleBehaviorTarget
{
	GENERATED_BODY()
public:

	friend UMirrorOperatorFactory;

	UMirrorTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	void RequestAction(EMirrorToolAction ActionType);

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IClickSequenceBehaviorTarget implementation
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;
protected:

	UPROPERTY()
	TObjectPtr<UMirrorToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UMirrorToolActionPropertySet> ToolActions = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UDynamicMeshReplacementChangeTarget>> MeshesToMirror;

	UPROPERTY()
	TArray<TObjectPtr<UMeshOpPreviewWithBackgroundCompute>> Previews;

	FVector3d MirrorPlaneOrigin = FVector3d::Zero();
	FVector3d MirrorPlaneNormal = FVector3d::UnitZ();

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic;

	EMirrorToolAction PendingAction;
	FBox CombinedBounds;
	void ApplyAction(EMirrorToolAction ActionType);

	void SetupPreviews();
	void GenerateAsset(const TArray<FDynamicMeshOpResult>& Results);

private:
	void CheckAndDisplayWarnings();
};