// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackedLevelActor/PackedLevelActorISMBuilder.h"

#if WITH_EDITOR

#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "PackedLevelActor/PackedLevelActor.h"

#include "Templates/TypeHash.h"

#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

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

	TArray<FTransform> InstanceTransforms;
	for (UActorComponent* Component : InComponents)
	{
		// If we have a ISM we need to add all instances
		if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(Component))
		{
			for (int32 InstanceIndex = 0; InstanceIndex < ISMComponent->GetInstanceCount(); ++InstanceIndex)
			{
				FTransform InstanceTransform;

				if (ensure(ISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, /*bWorldSpace=*/ true)))
				{
					// WorldSpace -> ActorSpace -> Apply pivot change
					InstanceTransforms.Add(InstanceTransform.GetRelativeTransform(ActorTransform) * InContext.GetRelativePivotTransform());
				}
			}
		}
		else // other subclasses are processed like regular UStaticMeshComponent
		{
			UStaticMeshComponent* StaticMeshComponent = CastChecked<UStaticMeshComponent>(Component);

			// WorldSpace -> ActorSpace -> Apply pivot change
			InstanceTransforms.Add(StaticMeshComponent->GetComponentTransform().GetRelativeTransform(ActorTransform) * InContext.GetRelativePivotTransform());
		}
	}

	FPackedLevelActorISMBuilderCluster* ISMCluster = (FPackedLevelActorISMBuilderCluster*)InClusterID.GetData();
	check(ISMCluster);

	TSubclassOf<UInstancedStaticMeshComponent> ComponentClass = UInstancedStaticMeshComponent::StaticClass();
	if (InstanceTransforms.Num() > 1 && ISMCluster->ISMDescriptor.StaticMesh && !ISMCluster->ISMDescriptor.StaticMesh->NaniteSettings.bEnabled)
	{
		// Use HISM for non-nanite when there is more than one transform (no use in using HISM cpu occlusion for a single instance)
		ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
	}
		
	UInstancedStaticMeshComponent* PackComponent = InPackingActor->AddPackedComponent<UInstancedStaticMeshComponent>(ComponentClass);
		
	PackComponent->AttachToComponent(InPackingActor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);

	ISMCluster->ISMDescriptor.InitComponent(PackComponent);

	PackComponent->AddInstances(InstanceTransforms, /*bShouldReturnIndices*/false, /*bWorldSpace*/false);
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