// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolBase.h"
#include "AvaShapeActor.h"
#include "AvaInteractiveToolsSettings.h"
#include "AvaViewportUtils.h"
#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "DynamicMeshes/AvaShape3DDynMeshBase.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"
#include "GameFramework/Actor.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"

UAvaShapesEditorShapeToolBase::UAvaShapesEditorShapeToolBase()
{
	ActorClass = AAvaShapeActor::StaticClass();
}

bool UAvaShapesEditorShapeToolBase::OnBegin()
{
	if (ShapeClass == nullptr)
	{
		return false;
	}

	return Super::OnBegin();
}

AActor* UAvaShapesEditorShapeToolBase::SpawnActor(TSubclassOf<AActor> InActorClass, EAvaViewportStatus InViewportStatus,
	const FVector2f& InViewportPosition, bool bInPreview, FString* InActorLabelOverride) const
{
	FString ActorLabelOverride = (InActorLabelOverride && !InActorLabelOverride->IsEmpty())
		? *InActorLabelOverride
		: GetActorNameOverride();

	AActor* NewActor = Super::SpawnActor(InActorClass, InViewportStatus, InViewportPosition, bInPreview, &ActorLabelOverride);

	if (AAvaShapeActor* ShapeActor = Cast<AAvaShapeActor>(NewActor))
	{
		UAvaShapeDynamicMeshBase* NewShape = NewObject<UAvaShapeDynamicMeshBase>(ShapeActor, ShapeClass);
		SetShapeSize(ShapeActor, FVector2D(DefaultDim, DefaultDim));
		InitShape(NewShape);

		ShapeActor->SetDynamicMesh(NewShape);

		SetToolkitSettingsObject(NewShape);
	}

	return NewActor;
}

FString UAvaShapesEditorShapeToolBase::GetActorNameOverride() const
{
	if (const UAvaShapeDynamicMeshBase* DefaultObject = ShapeClass.GetDefaultObject())
	{
		return DefaultObject->GetMeshName();
	}

	return "";
}

void UAvaShapesEditorShapeToolBase::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	InShape->SetParametricMaterial(UAvaShapeDynamicMeshBase::MESH_INDEX_PRIMARY, DefaultMaterialParams);
}

void UAvaShapesEditorShapeToolBase::SetShapeSize(AAvaShapeActor* InShapeActor, const FVector2D& InShapeSize) const
{
	if (!InShapeActor)
	{
		return;
	}

	UAvaShapeDynamicMeshBase* MeshBase = InShapeActor->GetDynamicMesh();

	if (!MeshBase)
	{
		return;
	}

	if (UAvaShape2DDynMeshBase* Mesh2D = Cast<UAvaShape2DDynMeshBase>(MeshBase))
	{
		Mesh2D->SetSize2D(InShapeSize);
	}
	else if (UAvaShape3DDynMeshBase* Mesh3D = Cast<UAvaShape3DDynMeshBase>(MeshBase))
	{
		Mesh3D->SetSize3D({DefaultDim, InShapeSize.X, InShapeSize.Y});
	}
	else
	{
		checkNoEntry();
	}
}

bool UAvaShapesEditorShapeToolBase::UseIdentityLocation() const
{
	if (!bPerformingDefaultAction)
	{
		return false;
	}

	if (!ShapeClass || ShapeClass->IsChildOf<UAvaShape2DDynMeshBase>())
	{
		return false;
	}

	return !IsMotionDesignViewport();
}

bool UAvaShapesEditorShapeToolBase::UseIdentityRotation() const
{
	return ConditionalIdentityRotation();
}
