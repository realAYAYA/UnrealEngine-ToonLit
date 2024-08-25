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
class MESHMODELINGTOOLSEXP_API UPhysicsInspectorTool : public UMultiSelectionMeshEditingTool, public IInteractiveToolManageGeometrySelectionAPI
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }

	// IInteractiveToolManageGeometrySelectionAPI -- this tool won't update external geometry selection or change selection-relevant mesh IDs
	virtual bool IsInputSelectionValidOnOutput() override
	{
		return true;
	}

	void OnCreatePhysics(UActorComponent* Component);

protected:

	UPROPERTY()
	TObjectPtr<UCollisionGeometryVisualizationProperties> VizSettings = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UPhysicsObjectToolPropertySet>> ObjectData;

protected:
	UPROPERTY()
	TArray<TObjectPtr<UPreviewGeometry>> PreviewElements;

private:
	// Helper to create or re-create preview geometry
	void InitializePreviewGeometry(bool bClearExisting);
	// Delegate to track when physics data may have been updated
	FDelegateHandle OnCreatePhysicsDelegateHandle;
	// A flag to track when the preview geometry needs to be re-initialized
	bool bUnderlyingPhysicsObjectsUpdated = false;
};
