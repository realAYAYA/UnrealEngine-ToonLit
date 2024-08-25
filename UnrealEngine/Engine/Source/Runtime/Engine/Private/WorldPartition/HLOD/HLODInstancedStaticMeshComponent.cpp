// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODInstancedStaticMeshComponent.h"
#include "WorldPartition/HLOD/HLODBuilder.h"

#include "Engine/StaticMesh.h"


UHLODInstancedStaticMeshComponent::UHLODInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

TUniquePtr<FISMComponentDescriptor> UHLODInstancedStaticMeshComponent::AllocateISMComponentDescriptor() const
{
	return MakeUnique<FHLODISMComponentDescriptor>();
}

FHLODISMComponentDescriptor::FHLODISMComponentDescriptor()
{
	ComponentClass = UHLODBuilder::GetInstancedStaticMeshComponentClass();
}

void FHLODISMComponentDescriptor::InitFrom(const UStaticMeshComponent* Component, bool bInitBodyInstance)
{
	Super::InitFrom(Component, bInitBodyInstance);

	// Stationnary can be considered as static for the purpose of HLODs
	if (Mobility == EComponentMobility::Stationary)
	{
		Mobility = EComponentMobility::Static;
	}
}


void FHLODISMComponentDescriptor::InitComponent(UInstancedStaticMeshComponent* ISMComponent) const
{
	Super::InitComponent(ISMComponent);

	if (ISMComponent->GetStaticMesh())
	{
		ISMComponent->SetForcedLodModel(ISMComponent->GetStaticMesh()->GetNumLODs());
	}
}

#endif
