// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InteractiveToolBuilder.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "GameFramework/Actor.h"
#include "Changes/ValueWatcher.h"

#include "AddPivotActorTool.generated.h"

class UDragAlignmentMechanic;
class UCombinedTransformGizmo;
class UTransformProxy;


UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UPivotActorTransformProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = PivotLocation)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = PivotLocation)
	FQuat Rotation = FQuat::Identity;
};


UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UAddPivotActorToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};

/** 
 * Given selected actors, creates an empty actor as the parent of those actors, at a location
 * specified using the gizmo. This is useful for creating a permanent alternate pivot to use in
 * animation.
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UAddPivotActorTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()
public:

	virtual void SetPivotRepositionMode(AActor* PivotActor) { ExistingPivotActor = PivotActor; }

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasAccept() const override { return true; }
	virtual bool HasCancel() const override { return true; }

	virtual void OnTick(float DeltaTime) override;

	// Uses the base class CanAccept

protected:

	UPROPERTY()
	TObjectPtr<UDragAlignmentMechanic> DragAlignmentMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> TransformGizmo = nullptr;

	UPROPERTY()
	TObjectPtr<UTransformProxy> TransformProxy = nullptr;

	UPROPERTY()
	TObjectPtr<UPivotActorTransformProperties> TransformProperties;

	TWeakObjectPtr<AActor> ExistingPivotActor = nullptr;

	TValueWatcher<FVector> GizmoPositionWatcher;
	TValueWatcher<FQuat> GizmoRotationWatcher;

	void UpdateGizmoFromProperties();
	void GizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform);

	FTransform ExistingPivotOriginalTransform;
};
