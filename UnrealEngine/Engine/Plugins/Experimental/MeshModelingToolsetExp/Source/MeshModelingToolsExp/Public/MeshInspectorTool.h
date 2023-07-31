// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SingleSelectionTool.h"
#include "InteractiveToolBuilder.h"
#include "Drawing/LineSetComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Properties/MeshMaterialProperties.h"
#include "PreviewMesh.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "MeshInspectorTool.generated.h"


// predeclarations
struct FMeshDescription;
class UMaterialInstanceDynamic;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshInspectorToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshInspectorProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Toggle visibility of all mesh edges */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bWireframe = true;

	/** Toggle visibility of open boundary edges */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bBoundaryEdges = true;

	/** Toggle visibility of bowtie vertices */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bBowtieVertices = true;

	/** Toggle visibility of polygon borders */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bPolygonBorders = false;

	/** Toggle visibility of UV seam edges */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bUVSeams = false;

	/** Toggle visibility of UV bowtie vertices */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bUVBowties = false;

	/** Toggle visibility of triangles with missing UVs */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bMissingUVs = false;

	/** Toggle visibility of Normal seam edges */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bNormalSeams = false;

	/** Toggle visibility of normal vectors */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bNormalVectors = false;

	/** Toggle visibility of tangent vectors */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bTangentVectors = false;

	/** Length of line segments representing normal vectors */
	UPROPERTY(EditAnywhere, Category = Options, meta = (EditCondition = "bNormalVectors", 
		UIMin="0", UIMax="400", ClampMin = "0", ClampMax = "1000000000.0"))
	float NormalLength = 5.0f;

	/** Length of line segments representing tangent vectors */
	UPROPERTY(EditAnywhere, Category = Options, meta = (EditCondition = "bTangentVectors", 
		UIMin = "0", UIMax = "400", ClampMin = "0", ClampMax = "1000000000.0"))
	float TangentLength = 5.0f;
};

/**
 * Mesh Inspector Tool for visualizing mesh information
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshInspectorTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()

public:
	UMeshInspectorTool();

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override;
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

public:

	virtual void IncreaseLineWidthAction();
	virtual void DecreaseLineWidthAction();

protected:

	UPROPERTY()
	TObjectPtr<UMeshInspectorProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UExistingMeshMaterialProperties> MaterialSettings = nullptr;


	float LineWidthMultiplier = 1.0f;

protected:
	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	UPROPERTY()
	TObjectPtr<ULineSetComponent> DrawnLineSet;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

	TArray<int> BoundaryEdges;
	TArray<int> BoundaryBowties;
	TArray<int> UVSeamEdges;
	TArray<int> UVBowties;
	TArray<int> NormalSeamEdges;
	TArray<int> GroupBoundaryEdges;
	TArray<int> MissingUVTriangleEdges;

	void UpdateVisualization();
	void Precompute();
};
