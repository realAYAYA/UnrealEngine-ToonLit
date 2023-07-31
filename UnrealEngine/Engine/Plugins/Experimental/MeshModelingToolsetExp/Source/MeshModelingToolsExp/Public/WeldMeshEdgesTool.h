// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "PreviewMesh.h"
#include "ModelingOperators.h"
#include "MeshOpPreviewHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "WeldMeshEdgesTool.generated.h"

// predeclarations
struct FMeshDescription;
class UMeshElementsVisualizer;
class UWeldMeshEdgesTool;
class FWeldMeshEdgesOp;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UWeldMeshEdgesToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UWeldMeshEdgesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Edges are considered matching if both pairs of endpoint vertices are closer than this distance */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "0.000001", UIMax = "0.01", ClampMin = "0.00000001", ClampMax = "1000.0"))
	float Tolerance = FMathf::ZeroTolerance;

	/** Only merge unambiguous pairs that have unique duplicate-edge matches */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bOnlyUnique = false;

	/** If enabled, after an initial attempt at Welding, attempt to resolve remaining open edges in T-junction configurations via edge splits, and then retry Weld */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bResolveTJunctions = false;

	/** Initial number of open boundary edges */
	UPROPERTY(VisibleAnywhere, Category = Statistics)
	int32 InitialEdges;

	/** Number of remaining open boundary edges */
	UPROPERTY(VisibleAnywhere, Category = Statistics)
	int32 RemainingEdges;
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UWeldMeshEdgesOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UWeldMeshEdgesTool> WeldMeshEdgesTool;
};


/**
 * Mesh Weld Edges Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UWeldMeshEdgesTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()
public:
	UWeldMeshEdgesTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	// update parameters in FWeldMeshEdgesOp based on current Settings
	void UpdateOpParameters(FWeldMeshEdgesOp& Op) const;

protected:
	UPROPERTY()
	TObjectPtr<UWeldMeshEdgesToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> PreviewCompute = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshElementsVisualizer> MeshElementsDisplay = nullptr;

	UPROPERTY()
	TObjectPtr<UWeldMeshEdgesOperatorFactory> OperatorFactory;

protected:

	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> SourceMesh;




};
