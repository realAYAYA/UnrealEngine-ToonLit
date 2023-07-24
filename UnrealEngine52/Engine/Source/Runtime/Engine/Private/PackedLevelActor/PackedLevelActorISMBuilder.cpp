// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackedLevelActor/PackedLevelActorISMBuilder.h"

#if WITH_EDITOR

#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "PackedLevelActor/PackedLevelActor.h"


#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "ISMPartition/ISMComponentBatcher.h"

FPackedLevelActorBuilderID FPackedLevelActorISMBuilder::BuilderID = 'ISMP';

FPackedLevelActorBuilderID FPackedLevelActorISMBuilder::GetID() const
{
	return BuilderID;
}

void FPackedLevelActorISMBuilder::GetPackClusters(FPackedLevelActorBuilderContext& InContext, AActor* InActor) const
{
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	InActor->GetComponents(StaticMeshComponents);

	for(UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (InContext.ShouldPackComponent(StaticMeshComponent))
		{
			FPackedLevelActorBuilderClusterID ClusterID(MakeUnique<FPackedLevelActorISMBuilderCluster>(GetID(), StaticMeshComponent));

			InContext.FindOrAddCluster(MoveTemp(ClusterID), StaticMeshComponent);
		}
	}
}

void FPackedLevelActorISMBuilder::PackActors(FPackedLevelActorBuilderContext& InContext, APackedLevelActor* InPackingActor, const FPackedLevelActorBuilderClusterID& InClusterID, const TArray<UActorComponent*>& InComponents) const
{
	check(InClusterID.GetBuilderID() == GetID());
	FTransform ActorTransform = InPackingActor->GetActorTransform();

	FPackedLevelActorISMBuilderCluster* ISMCluster = (FPackedLevelActorISMBuilderCluster*)InClusterID.GetData();
	check(ISMCluster);

	FISMComponentBatcher ISMComponentBatcher;
	ISMComponentBatcher.Append(InComponents, [&ActorTransform, &InContext](const FTransform& InTransform) { return InTransform.GetRelativeTransform(ActorTransform) * InContext.GetRelativePivotTransform() * ActorTransform; });

	TSubclassOf<UInstancedStaticMeshComponent> ComponentClass = UInstancedStaticMeshComponent::StaticClass();
	if (ISMComponentBatcher.GetNumInstances() > 1 && ISMCluster->ISMDescriptor.StaticMesh && !ISMCluster->ISMDescriptor.StaticMesh->NaniteSettings.bEnabled)
	{
		// Use HISM for non-nanite when there is more than one transform (no use in using HISM cpu occlusion for a single instance)
		ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
	}

	UInstancedStaticMeshComponent* PackComponent = InPackingActor->AddPackedComponent<UInstancedStaticMeshComponent>(ComponentClass);
	PackComponent->AttachToComponent(InPackingActor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);

	// Initialize the ISM properties using the ISM descriptor
	ISMCluster->ISMDescriptor.InitComponent(PackComponent);

	// Initialize the ISM instances using the ISM batcher
	ISMComponentBatcher.InitComponent(PackComponent);

	PackComponent->RegisterComponent();
}

FPackedLevelActorISMBuilderCluster::FPackedLevelActorISMBuilderCluster(FPackedLevelActorBuilderID InBuilderID, UStaticMeshComponent* InComponent)
	: FPackedLevelActorBuilderCluster(InBuilderID)
{
	ISMDescriptor.InitFrom(InComponent, /** bInitBodyInstance= */ false);
	// ComponentClass will be determined based on StaticMesh nanite settings and number of instances and we don't want it to be part of the cluster hash
	ISMDescriptor.ComponentClass = nullptr;
	// Component descriptor should be considered hidden if original actor owner was.
	ISMDescriptor.bHiddenInGame |= InComponent->GetOwner()->IsHidden();
	ISMDescriptor.BodyInstance.CopyRuntimeBodyInstancePropertiesFrom(&InComponent->BodyInstance);
	ISMDescriptor.ComputeHash();
}

uint32 FPackedLevelActorISMBuilderCluster::ComputeHash() const
{
	return HashCombine(FPackedLevelActorBuilderCluster::ComputeHash(), ISMDescriptor.Hash);
}

bool FPackedLevelActorISMBuilderCluster::Equals(const FPackedLevelActorBuilderCluster& InOther) const
{
	if (!FPackedLevelActorBuilderCluster::Equals(InOther))
	{
		return false;
	}

	const FPackedLevelActorISMBuilderCluster& ISMOther = (const FPackedLevelActorISMBuilderCluster&)InOther;
	return ISMDescriptor == ISMOther.ISMDescriptor;
}

#endif
