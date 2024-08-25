// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"


DEFINE_LOG_CATEGORY(LogInstancedActors);

//-----------------------------------------------------------------------------
// FInstancedActorsTagSet
//-----------------------------------------------------------------------------
FInstancedActorsTagSet::FInstancedActorsTagSet(const FGameplayTagContainer& InTags)
{
	TArray<FGameplayTag> SortedTags;
	InTags.GetGameplayTagArray(SortedTags);
	SortedTags.Sort();

	Tags = FGameplayTagContainer::CreateFromArray(SortedTags);

	Hash = 0;
	for (const FGameplayTag& Tag : SortedTags)
	{
		Hash = HashCombine(Hash, GetTypeHash(Tag));
	}
}

//-----------------------------------------------------------------------------
// FStaticMeshInstanceVisualizationDesc
//-----------------------------------------------------------------------------
FInstancedActorsVisualizationDesc::FInstancedActorsVisualizationDesc(const FInstancedActorsSoftVisualizationDesc& SoftVisualizationDesc)
{
	for (const FSoftISMComponentDescriptor& SoftISMComponentDescriptor : SoftVisualizationDesc.ISMComponentDescriptors)
	{
		// Calls FISMComponentDescriptor(FSoftISMComponentDescriptor&) which will LoadSynchronous any soft paths
		ISMComponentDescriptors.Emplace(SoftISMComponentDescriptor);
	}
}

FInstancedActorsVisualizationDesc FInstancedActorsVisualizationDesc::FromActor(const AActor& ExemplarActor
	, const FInstancedActorsVisualizationDesc::FAdditionalSetupStepsFunction& AdditionalSetupSteps)
{
	FInstancedActorsVisualizationDesc Visualization;

	if (const UStaticMeshComponent* SourceStaticMeshComponent = ExemplarActor.GetComponentByClass<UStaticMeshComponent>())
	{
		const UStaticMesh* StaticMesh = SourceStaticMeshComponent->GetStaticMesh();
		if (!IsValid(StaticMesh))
		{
			// No mesh = no visualization
			return MoveTemp(Visualization);
		}

		FISMComponentDescriptor& ISMComponentDescriptor = Visualization.ISMComponentDescriptors.AddDefaulted_GetRef();
		ISMComponentDescriptor.InitFrom(SourceStaticMeshComponent);

		AdditionalSetupSteps(ExemplarActor, ISMComponentDescriptor, Visualization);
	}

	return MoveTemp(Visualization);
}

FStaticMeshInstanceVisualizationDesc FInstancedActorsVisualizationDesc::ToMassVisualizationDesc() const
{
	FStaticMeshInstanceVisualizationDesc OutMassVisualizationDesc;

	for (const FISMComponentDescriptor& ISMComponentDescriptor : ISMComponentDescriptors)
	{
		if (!ensure(IsValid(ISMComponentDescriptor.StaticMesh)))
		{
			continue;
		}

		FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = OutMassVisualizationDesc.Meshes.AddDefaulted_GetRef();
		MeshDesc.Mesh = ISMComponentDescriptor.StaticMesh;
		MeshDesc.bCastShadows = ISMComponentDescriptor.bCastShadow;
		MeshDesc.Mobility = EComponentMobility::Stationary;
		MeshDesc.MaterialOverrides = ISMComponentDescriptor.OverrideMaterials;
		MeshDesc.ISMComponentClass = UInstancedStaticMeshComponent::StaticClass();
	}

	return MoveTemp(OutMassVisualizationDesc);
}

//-----------------------------------------------------------------------------
// FInstancedActorsSoftVisualizationDesc
//-----------------------------------------------------------------------------
FInstancedActorsSoftVisualizationDesc::FInstancedActorsSoftVisualizationDesc(const FInstancedActorsVisualizationDesc& VisualizationDesc)
{
	for (const FISMComponentDescriptor& ISMComponentDescriptor : VisualizationDesc.ISMComponentDescriptors)
	{
		ISMComponentDescriptors.Emplace(ISMComponentDescriptor);
	}
}

void FInstancedActorsSoftVisualizationDesc::GetAssetsToLoad(TArray<FSoftObjectPath>& OutAssetsToLoad) const
{
	for (const FSoftISMComponentDescriptor& ISMComponentDescriptor : ISMComponentDescriptors)
	{
		if (ISMComponentDescriptor.StaticMesh.IsPending())
		{
			OutAssetsToLoad.Add(ISMComponentDescriptor.StaticMesh.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<UMaterialInterface>& OverrideMaterial : ISMComponentDescriptor.OverrideMaterials)
		{
			if (OverrideMaterial.IsPending())
			{
				OutAssetsToLoad.Add(OverrideMaterial.ToSoftObjectPath());
			}
		}
		if (ISMComponentDescriptor.OverlayMaterial.IsPending())
		{
			OutAssetsToLoad.Add(ISMComponentDescriptor.OverlayMaterial.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<URuntimeVirtualTexture>& RuntimeVirtualTexture : ISMComponentDescriptor.RuntimeVirtualTextures)
		{
			if (RuntimeVirtualTexture.IsPending())
			{
				OutAssetsToLoad.Add(RuntimeVirtualTexture.ToSoftObjectPath());
			}
		}
	}
}
