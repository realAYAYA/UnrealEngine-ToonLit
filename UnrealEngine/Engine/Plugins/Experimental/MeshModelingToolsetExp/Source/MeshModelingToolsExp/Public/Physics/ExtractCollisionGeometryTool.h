// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Physics/CollisionPropertySets.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "ExtractCollisionGeometryTool.generated.h"

class UPreviewGeometry;
class UPreviewMesh;

UCLASS()
class MESHMODELINGTOOLSEXP_API UExtractCollisionGeometryToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};


UENUM()
enum class EExtractCollisionOutputType : uint8
{
	/** Simple Collision shapes (Box, Sphere, Capsule, Convex) */
	Simple = 0,
	/** Complex Collision Mesh */
	Complex = 1
};


UCLASS()
class MESHMODELINGTOOLSEXP_API UExtractCollisionToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Type of collision geometry to convert to Mesh */
	UPROPERTY(EditAnywhere, Category = Options)
	EExtractCollisionOutputType CollisionType = EExtractCollisionOutputType::Simple;

	/** Whether or not to weld coincident border edges of the Complex Collision Mesh (if possible) */
	UPROPERTY(EditAnywhere, Category = Options, Meta = (EditCondition = "CollisionType == EExtractCollisionOutputType::Complex"))
	bool bWeldEdges = true;

	/** Whether or not to generate a seperate Mesh Object for each Simple Collision Shape  */
	UPROPERTY(EditAnywhere, Category = Options, Meta = (EditCondition = "CollisionType == EExtractCollisionOutputType::Simple"))
	bool bOutputSeparateMeshes = true;

	/** Show/Hide a preview of the generated mesh (overlaps source mesh) */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowPreview = false;

	/** Show/Hide input mesh */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowInputMesh = true;
};



/**
 * Mesh Inspector Tool for visualizing mesh information
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UExtractCollisionGeometryTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

protected:

	/** Property set for type of output object (StaticMesh, Volume, etc) */
	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

	UPROPERTY()
	TObjectPtr<UExtractCollisionToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UCollisionGeometryVisualizationProperties> VizSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UPhysicsObjectToolPropertySet> ObjectProps;

protected:
	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewElements;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	// these are TSharedPtr because TPimplPtr cannot currently be added to a TArray?
	TSharedPtr<FPhysicsDataCollection> PhysicsInfo;

	TArray<TSharedPtr<UE::Geometry::FDynamicMesh3>> CurrentMeshParts;
	UE::Geometry::FDynamicMesh3 CurrentMesh;
	bool bResultValid = false;
	void RecalculateMesh_Simple();
	void RecalculateMesh_Complex();

	bool bVisualizationDirty = false;
	void UpdateVisualization();
};
