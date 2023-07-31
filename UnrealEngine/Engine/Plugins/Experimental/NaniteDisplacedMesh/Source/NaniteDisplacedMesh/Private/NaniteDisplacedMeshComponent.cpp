// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ComponentRecreateRenderStateContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NaniteDisplacedMeshComponent)

int32 GRenderNaniteDisplacedMesh = 1;
FAutoConsoleVariableRef CVarRenderNaniteDisplacedMesh(
	TEXT("r.Nanite.DisplacedMesh"),
	GRenderNaniteDisplacedMesh,
	TEXT("Render Nanite displaced meshes"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

void UNaniteDisplacedMeshComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (GIsEditor)
	{
		BindCallback();
	}
#endif
}

void UNaniteDisplacedMeshComponent::BeginDestroy()
{
#if WITH_EDITOR
	UnbindCallback();
#endif

	Super::BeginDestroy();
}

void UNaniteDisplacedMeshComponent::OnRegister()
{
	Super::OnRegister();
}

void UNaniteDisplacedMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

const Nanite::FResources* UNaniteDisplacedMeshComponent::GetNaniteResources() const
{
	// TODO: Refactor API to support also overriding the mesh section info

	if (GRenderNaniteDisplacedMesh == 0)
	{
		return nullptr;
	}

	if (IsValid(DisplacedMesh) && DisplacedMesh->HasValidNaniteData())
	{
		return DisplacedMesh->GetNaniteData();
	}

	// If the displaced mesh does not have valid Nanite data, try the SMC's static mesh.
	if (GetStaticMesh() && GetStaticMesh()->GetRenderData())
	{
		return &GetStaticMesh()->GetRenderData()->NaniteResources;
	}

	return nullptr;
}

FPrimitiveSceneProxy* UNaniteDisplacedMeshComponent::CreateSceneProxy()
{
	return Super::CreateSceneProxy();
}

#if WITH_EDITOR

static const FName NAME_DisplacedMesh = GET_MEMBER_NAME_CHECKED(UNaniteDisplacedMeshComponent, DisplacedMesh);

void UNaniteDisplacedMeshComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (GIsEditor)
	{
		const FName PropertyName = PropertyAboutToChange ? PropertyAboutToChange->GetFName() : NAME_None;
		if (PropertyName == NAME_DisplacedMesh)
		{
			UnbindCallback();
		}
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UNaniteDisplacedMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GIsEditor)
	{
		const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
		
		if (PropertyName == NAME_DisplacedMesh)
		{
			BindCallback();
		}
	}
}

void UNaniteDisplacedMeshComponent::PostEditUndo()
{
	Super::PostEditUndo();
}

void UNaniteDisplacedMeshComponent::OnRebuild()
{
	MarkRenderStateDirty();
}

void UNaniteDisplacedMeshComponent::UnbindCallback()
{
	if (DisplacedMesh)
	{
		DisplacedMesh->UnregisterOnRenderingDataChanged(this);
	}
}

void UNaniteDisplacedMeshComponent::BindCallback()
{
	if (DisplacedMesh)
	{
		DisplacedMesh->RegisterOnRenderingDataChanged(
			UNaniteDisplacedMesh::FOnRebuild::CreateUObject(this, &UNaniteDisplacedMeshComponent::OnRebuild)
		);
	}
}

#endif

