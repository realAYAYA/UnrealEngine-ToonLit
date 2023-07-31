// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "Physics/CollisionPropertySets.h"
#include "PhysicsInspectorTool.generated.h"

class UPreviewGeometry;

UCLASS()
class MESHMODELINGTOOLSEXP_API UPhysicsInspectorToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};




/**
 * Mesh Inspector Tool for visualizing mesh information
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPhysicsInspectorTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }

protected:

	UPROPERTY()
	TObjectPtr<UCollisionGeometryVisualizationProperties> VizSettings = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UPhysicsObjectToolPropertySet>> ObjectData;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LineMaterial = nullptr;

protected:
	UPROPERTY()
	TArray<TObjectPtr<UPreviewGeometry>> PreviewElements;

	// these are TSharedPtr because TPimplPtr cannot currently be added to a TArray?
	TArray<TSharedPtr<FPhysicsDataCollection>> PhysicsInfos;

	void InitializeGeometry(const FPhysicsDataCollection& PhysicsData, UPreviewGeometry* PreviewGeom);
	void InitializeObjectProperties(const FPhysicsDataCollection& PhysicsData, UPhysicsObjectToolPropertySet* PropSet);

	bool bVisualizationDirty = false;
	void UpdateVisualization();
};
