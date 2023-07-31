// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModelingOperators.h" //IDynamicMeshOperatorFactory
#include "InteractiveTool.h" //UInteractiveToolPropertySet
#include "InteractiveToolBuilder.h" //UInteractiveToolBuilder
#include "InteractiveToolChange.h" //FToolCommandChange
#include "SingleSelectionTool.h" //USingleSelectionTool
#include "Operations/FFDLattice.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "Solvers/ConstrainedMeshDeformer.h"

#include "LatticeDeformerTool.generated.h"

class ULatticeControlPointsMechanic;
class UMeshOpPreviewWithBackgroundCompute;

UCLASS()
class MESHMODELINGTOOLSEXP_API ULatticeDeformerToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


UENUM()
enum class ELatticeInterpolationType : uint8
{
	/** Use trilinear interpolation to get new mesh vertex positions from the lattice */
	Linear UMETA(DisplayName = "Linear"),

	/** Use tricubic interpolation to get new mesh vertex positions from the lattice */
	Cubic UMETA(DisplayName = "Cubic")
};

UENUM()
enum class ELatticeDeformerToolAction  : uint8
{
	NoAction,
	Constrain,
	ClearConstraints
};

UCLASS()
class MESHMODELINGTOOLSEXP_API ULatticeDeformerToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ULatticeDeformerTool> ParentTool;
	void Initialize(ULatticeDeformerTool* ParentToolIn) { ParentTool = ParentToolIn; }
	void PostAction(ELatticeDeformerToolAction Action);

	/** Number of lattice vertices along the X axis */
	UPROPERTY(EditAnywhere, Category = Resolution, meta = (UIMin = "2", ClampMin = "2", UIMax = "25", ClampMax = "40", EditCondition = "bCanChangeResolution", HideEditConditionToggle))
	int XAxisResolution = 5;

	/** Number of lattice vertices along the Y axis */
	UPROPERTY(EditAnywhere, Category = Resolution, meta = (UIMin = "2", ClampMin = "2", UIMax = "25", ClampMax = "40", EditCondition = "bCanChangeResolution", HideEditConditionToggle))
	int YAxisResolution = 5;

	/** Number of lattice vertices along the Z axis */
	UPROPERTY(EditAnywhere, Category = Resolution, meta = (UIMin = "2", ClampMin = "2", UIMax = "25", ClampMax = "40", EditCondition = "bCanChangeResolution", HideEditConditionToggle))
	int ZAxisResolution = 5;

	/** Relative distance the lattice extends from the mesh */
	UPROPERTY(EditAnywhere, Category = Resolution, meta = (UIMin = "0.01", ClampMin = "0.01", UIMax = "2", ClampMax = "5", EditCondition = "bCanChangeResolution", HideEditConditionToggle))
	float Padding = 0.01;

	/** Whether to use linear or cubic interpolation to get new mesh vertex positions from the lattice */
	UPROPERTY(EditAnywhere, Category = Interpolation )
	ELatticeInterpolationType InterpolationType = ELatticeInterpolationType::Linear;

	/** Whether to use approximate new vertex normals using the deformer */
	UPROPERTY(EditAnywhere, Category = Interpolation)
	bool bDeformNormals = false;

	// Not user visible - used to disallow changing the lattice resolution after deformation
	UPROPERTY(meta = (TransientToolProperty))
	bool bCanChangeResolution = true;

	/** Whether the gizmo's axes remain aligned with world axes or rotate as the gizmo is transformed */
	UPROPERTY(EditAnywhere, Category = Gizmo)
	EToolContextCoordinateSystem GizmoCoordinateSystem = EToolContextCoordinateSystem::Local;

	/** If Set Pivot Mode is active, the gizmo can be repositioned without moving the selected lattice points */
	UPROPERTY(EditAnywhere, Category = Gizmo)
	bool bSetPivotMode = false;

	/** Whether to use soft deformation of the lattice */
	UPROPERTY(EditAnywhere, Category = Deformation)
	bool bSoftDeformation = false;

	/** Constrain selected lattice points */
	UFUNCTION(CallInEditor, Category = Deformation, meta = (DisplayName = "Constrain"))
	void Constrain() 
	{
		PostAction(ELatticeDeformerToolAction::Constrain);
	}

	/** Clear all constrained lattice points */
	UFUNCTION(CallInEditor, Category = Deformation, meta = (DisplayName = "Clear Constraints"))
	void ClearConstraints()
	{
		PostAction(ELatticeDeformerToolAction::ClearConstraints);
	}

};


UCLASS()
class MESHMODELINGTOOLSEXP_API ULatticeDeformerOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<ULatticeDeformerTool> LatticeDeformerTool;
};


/** Deform a mesh using a regular hexahedral lattice */
UCLASS()
class MESHMODELINGTOOLSEXP_API ULatticeDeformerTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()

public:

	virtual ~ULatticeDeformerTool() = default;

	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	UE::Geometry::FVector3i GetLatticeResolution() const;

protected:

	// Input mesh
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;

	TSharedPtr<UE::Geometry::FFFDLattice, ESPMode::ThreadSafe> Lattice;

	UPROPERTY()
	TObjectPtr<ULatticeControlPointsMechanic> ControlPointsMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<ULatticeDeformerToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview = nullptr;

	UPROPERTY()
	bool bLatticeDeformed = false;

	bool bShouldRebuild = false;

	// Create and store an FFFDLattice. Pass out the lattice's positions and edges.
	void InitializeLattice(TArray<FVector3d>& OutLatticePoints, TArray<UE::Geometry::FVector2i>& OutLatticeEdges);

	void StartPreview();

	TUniquePtr<UE::Solvers::IConstrainedMeshSolver> DeformationSolver;
	TPimplPtr<UE::Geometry::FDynamicGraph3d> LatticeGraph;

	TMap<int32, FVector3d> ConstrainedLatticePoints;
	void ConstrainSelectedPoints();
	void ClearConstrainedPoints();
	void UpdateMechanicColorOverrides();
	void ResetConstrainedPoints();

	void RebuildDeformer();
	void SoftDeformLattice();

	int32 CurrentChangeStamp = 0;

	ELatticeDeformerToolAction PendingAction = ELatticeDeformerToolAction::NoAction;
	void RequestAction(ELatticeDeformerToolAction Action);
	void ApplyAction(ELatticeDeformerToolAction Action);

	friend class ULatticeDeformerOperatorFactory;
	friend class ULatticeDeformerToolProperties;
	friend class FLatticeDeformerToolConstrainedPointsChange;
};


// Set of constrained points change
class MESHMODELINGTOOLSEXP_API FLatticeDeformerToolConstrainedPointsChange : public FToolCommandChange
{
public:

	FLatticeDeformerToolConstrainedPointsChange(const TMap<int, FVector3d>& PrevConstrainedLatticePointsIn,
												const TMap<int, FVector3d>& NewConstrainedLatticePointsIn,
												int32 ChangeStampIn) :
		PrevConstrainedLatticePoints(PrevConstrainedLatticePointsIn),
		NewConstrainedLatticePoints(NewConstrainedLatticePointsIn),
		ChangeStamp(ChangeStampIn)
	{}

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;

	virtual bool HasExpired(UObject* Object) const override
	{
		return Cast<ULatticeDeformerTool>(Object)->CurrentChangeStamp != ChangeStamp;
	}

	virtual FString ToString() const override;

protected:

	TMap<int, FVector3d> PrevConstrainedLatticePoints;
	TMap<int, FVector3d> NewConstrainedLatticePoints;
	int32 ChangeStamp;
};
