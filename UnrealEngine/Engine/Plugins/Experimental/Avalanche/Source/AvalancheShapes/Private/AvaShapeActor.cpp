// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaShapeActor.h"

#include "Components/DynamicMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"

const FName AAvaShapeActor::ShapeComponentName("Shape Component");

// Sets default values
AAvaShapeActor::AAvaShapeActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused   = true;

	ShapeMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(ShapeComponentName);
	ShapeMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ShapeMeshComponent->SetGenerateOverlapEvents(true);
	SetRootComponent(ShapeMeshComponent);

	bFinishedCreation = false;
}

void AAvaShapeActor::SetDynamicMesh(UAvaShapeDynamicMeshBase* NewDynamicMesh)
{
	DynamicMesh = NewDynamicMesh;
	DynamicMesh->SetFlags(RF_Transactional);

	AddInstanceComponent(DynamicMesh);

	NewDynamicMesh->OnComponentCreated();
	NewDynamicMesh->RegisterComponent();

#if WITH_EDITOR
	RerunConstructionScripts();
#endif
}

#if WITH_EDITOR
FString AAvaShapeActor::GetDefaultActorLabel() const
{
	return DynamicMesh
		? DynamicMesh->GetMeshName()
		: TEXT("Shape");
}

void AAvaShapeActor::PostEditUndo()
{
	// fix to restore dynamic mesh
	if (DynamicMesh && !IsValid(DynamicMesh))
	{
		DynamicMesh->ClearGarbage();
	}
	
	Super::PostEditUndo();
}
#endif

FAvaColorChangeData AAvaShapeActor::GetColorData() const
{
	if (!DynamicMesh)
	{
		return FAvaColorChangeData(EAvaColorStyle::None, FLinearColor::White, FLinearColor::White, true);
	}

	return DynamicMesh->GetActiveColor();
}

void AAvaShapeActor::SetColorData(const FAvaColorChangeData& NewColorData)
{
	if (!DynamicMesh)
	{
		return;
	}

	DynamicMesh->OnColorPicked(NewColorData);
}

TArray<FAvaSnapPoint> AAvaShapeActor::GetLocalSnapPoints() const
{
	if (DynamicMesh)
	{
		return DynamicMesh->GetLocalSnapPoints();
	}

	return {};
}

UObject* AAvaShapeActor::GetModeDetailsObject_Implementation() const
{
	return DynamicMesh.Get();
}
