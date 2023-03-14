// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveGizmo.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "MeshOpPreviewHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Changes/DynamicMeshChangeTarget.h"
#include "BaseTools/SingleClickTool.h"
#include "Mechanics/ConstructionPlaneMechanic.h"
#include "PlaneCutTool.generated.h"

class UPlaneCutTool;


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPlaneCutToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};



/**
* Properties controlling how changes are baked out to static meshes on tool accept
*/
UCLASS()
class MESHMODELINGTOOLSEXP_API UAcceptOutputProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	/** If true, meshes cut into multiple pieces will be saved as separate assets on 'accept'. */
	UPROPERTY(EditAnywhere, Category = ToolOutputOptions)
	bool bExportSeparatedPiecesAsNewMeshAssets = true;
};






/**
 * Standard properties of the plane cut operation
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPlaneCutToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPlaneCutToolProperties()
	{}

	/** If true, both halves of the cut are computed */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bKeepBothHalves = false;

	/** If keeping both halves, separate the two pieces by this amount */
	UPROPERTY(EditAnywhere, Category = Options, meta = (EditCondition = "bKeepBothHalves == true", UIMin = "0", ClampMin = "0") )
	float SpacingBetweenHalves = 0;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowPreview = true;

	/** If true, the cut surface is filled with simple planar hole fill surface(s) */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bFillCutHole = true;

	/** If true, will attempt to fill cut holes even if they're ill-formed (e.g. because they connect to pre-existing holes in the geometry) */
	UPROPERTY(EditAnywhere, Category = Options, AdvancedDisplay, meta = (EditCondition = "bFillCutHole"))
	bool bFillSpans = false;
};



UENUM()
enum class EPlaneCutToolActions
{
	NoAction,
	Cut,
	FlipPlane
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UPlaneCutOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UPlaneCutTool> CutTool;

	int ComponentIndex;
};

/**
 * Simple Mesh Plane Cutting Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPlaneCutTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()

public:

	friend UPlaneCutOperatorFactory;

	UPlaneCutTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:

	UPROPERTY()
	TObjectPtr<UPlaneCutToolProperties> BasicProperties;

	UPROPERTY()
	TObjectPtr<UAcceptOutputProperties> AcceptProperties;

	UPROPERTY()
	TArray<TObjectPtr<UMeshOpPreviewWithBackgroundCompute>> Previews;


	/// Action buttons.
	/// Note these set a flag to call the action later (in OnTick)
	/// Otherwise, the actions in undo history will end up being generically named by an outer UI handler transaction

	/** Cut with the current plane without exiting the tool (Hotkey: T)*/
	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Cut"))
	void Cut()
	{
		PendingAction = EPlaneCutToolActions::Cut;
	}

	/** Flip the cutting plane (Hotkey: R) */
	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Flip Plane"))
	void FlipPlane()
	{
		PendingAction = EPlaneCutToolActions::FlipPlane;
	}

protected:

	UPROPERTY()
	TArray<TObjectPtr<UDynamicMeshReplacementChangeTarget>> MeshesToCut;

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;

	// Cutting plane
	UE::Geometry::FFrame3d CutPlaneWorld;

	// UV Scale factor is cached based on the bounding box of the mesh before any cuts are performed, so you don't get inconsistent UVs if you multi-cut the object to smaller sizes
	TArray<float> MeshUVScaleFactor;

	FViewCameraState CameraState;

	EPlaneCutToolActions PendingAction = EPlaneCutToolActions::NoAction;


	void DoCut();
	void DoFlipPlane();

	void SetupPreviews();

	void InvalidatePreviews();

	void GenerateAsset(const TArray<FDynamicMeshOpResult>& Results);
};
