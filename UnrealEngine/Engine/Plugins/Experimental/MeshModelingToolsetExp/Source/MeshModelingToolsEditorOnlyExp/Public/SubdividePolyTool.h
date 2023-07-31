// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Operations/SubdividePoly.h"
#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "ModelingOperators.h"
#include "SingleSelectionTool.h"
#include "Components/DynamicMeshComponent.h"
#include "MeshOpPreviewHelpers.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "SubdividePolyTool.generated.h"

class USubdividePolyTool;
class UPreviewGeometry;

/**
 * Tool builder
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USubdividePolyToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Properties
 */

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USubdividePolyToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category=Settings, meta = (UIMin = "1", ClampMin = "1"))
	int SubdivisionLevel = 3;

	// Controls whether the user can select Catmull-Clark or is forced to use Loop
	UPROPERTY(meta = (TransientToolProperty))
	bool bCatmullClarkOK = true;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "bCatmullClarkOK", HideEditConditionToggle))
	ESubdivisionScheme SubdivisionScheme = ESubdivisionScheme::CatmullClark;

	UPROPERTY(EditAnywhere, Category=Settings)
	ESubdivisionOutputNormals NormalComputationMethod = ESubdivisionOutputNormals::Generated;

	UPROPERTY(EditAnywhere, Category=Settings)
	ESubdivisionOutputUVs UVComputationMethod = ESubdivisionOutputUVs::Interpolated;

	/** Assign a new PolyGroup ID to each newly created face */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bNewPolyGroups = false;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bRenderGroups = true;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bRenderCage = true;

};


/**
 * Tool actual
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USubdividePolyTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()

public:

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	void OnTick(float DeltaTime);

protected:

	friend class USubdividePolyOperatorFactory;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh = nullptr;

	UPROPERTY()
	TObjectPtr<USubdividePolyToolProperties> Properties = nullptr;

	// Input mesh
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;

	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewGeometry = nullptr;

	bool bPreviewGeometryNeedsUpdate;
	void CreateOrUpdatePreviewGeometry();

	// Determine if the mesh group topology can be used for Catmull-Clark or Bilinear subdivision. If not, we can only 
	// Loop subdivision on the original triangle mesh.
	bool CheckGroupTopology(FText& Message);

	void CapSubdivisionLevel(ESubdivisionScheme Scheme, int DesiredLevel);

	// Tool message when no explicit parameter-related warnings are being pushed.  
	FText PersistentErrorMessage;
};

