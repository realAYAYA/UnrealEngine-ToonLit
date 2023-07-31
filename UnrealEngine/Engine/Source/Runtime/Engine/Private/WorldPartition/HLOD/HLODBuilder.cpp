// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODBuilder.h"

#include "AssetCompilingManager.h"
#include "Engine/HLODProxy.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ISMPartition/ISMComponentDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODBuilder)


DEFINE_LOG_CATEGORY(LogHLODBuilder);


UHLODBuilder::UHLODBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNullHLODBuilder::UNullHLODBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHLODBuilderSettings::UHLODBuilderSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

TSubclassOf<UHLODBuilderSettings> UHLODBuilder::GetSettingsClass() const
{
	return UHLODBuilderSettings::StaticClass();
}

void UHLODBuilder::SetHLODBuilderSettings(const UHLODBuilderSettings* InHLODBuilderSettings)
{
	check(InHLODBuilderSettings->IsA(GetSettingsClass()));
	HLODBuilderSettings = InHLODBuilderSettings;
}

bool UHLODBuilder::RequiresCompiledAssets() const
{
	return true;
}

bool UHLODBuilder::RequiresWarmup() const
{
	return true;
}

static TArray<UActorComponent*> GatherHLODRelevantComponents(const TArray<AActor*>& InSourceActors)
{
	TArray<UActorComponent*> HLODRelevantComponents;

	for (AActor* Actor : InSourceActors)
	{
		if (!Actor || !Actor->IsHLODRelevant())
		{
			continue;
		}

		for (UActorComponent* SubComponent : Actor->GetComponents())
		{
			if (SubComponent && SubComponent->IsHLODRelevant())
			{
				HLODRelevantComponents.Add(SubComponent);
			}
		}
	}

	return HLODRelevantComponents;
}

uint32 UHLODBuilder::ComputeHLODHash(const UActorComponent* InSourceComponent) const
{
	uint32 ComponentCRC = 0;

	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(const_cast<UActorComponent*>(InSourceComponent)))
	{
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - Component \'%s\' from actor \'%s\'"), *StaticMeshComponent->GetName(), *StaticMeshComponent->GetOwner()->GetName());

		// CRC component
		ComponentCRC = UHLODProxy::GetCRC(StaticMeshComponent);
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - Static Mesh Component (%s) = %x"), *StaticMeshComponent->GetName(), ComponentCRC);

		// CRC static mesh
		if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			ComponentCRC = UHLODProxy::GetCRC(StaticMesh, ComponentCRC);
			UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - Static Mesh (%s) = %x"), *StaticMesh->GetName(), ComponentCRC);
		}

		// CRC materials
		const int32 NumMaterials = StaticMeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			UMaterialInterface* MaterialInterface = StaticMeshComponent->GetMaterial(MaterialIndex);
			if (MaterialInterface)
			{
				ComponentCRC = UHLODProxy::GetCRC(MaterialInterface, ComponentCRC);
				UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - Material (%s) = %x"), *MaterialInterface->GetName(), ComponentCRC);

				TArray<UTexture*> Textures;
				MaterialInterface->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
				for (UTexture* Texture : Textures)
				{
					ComponentCRC = UHLODProxy::GetCRC(Texture, ComponentCRC);
					UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - Texture (%s) = %x"), *Texture->GetName(), ComponentCRC);
				}
			}
		}
	}
	else
	{
		ComponentCRC = FMath::Rand();
		UE_LOG(LogHLODBuilder, Warning, TEXT("Can't compute HLOD hash for component of type %s, assuming it is dirty."), *InSourceComponent->GetClass()->GetName());
		
	}

	return ComponentCRC;
}

uint32 UHLODBuilder::ComputeHLODHash(const TArray<AActor*>& InSourceActors)
{
	// We get the CRC of each component
	TArray<uint32> ComponentsCRCs;

	for (UActorComponent* SourceComponent : GatherHLODRelevantComponents(InSourceActors))
	{
		TSubclassOf<UHLODBuilder> HLODBuilderClass = SourceComponent->GetCustomHLODBuilderClass();
		if (!HLODBuilderClass)
		{
			HLODBuilderClass = UHLODBuilder::StaticClass();
		}

		uint32 ComponentHash = HLODBuilderClass->GetDefaultObject<UHLODBuilder>()->ComputeHLODHash(SourceComponent);
		ComponentsCRCs.Add(ComponentHash);
	}

	// Sort the components CRCs to ensure the order of components won't have an impact on the final CRC
	ComponentsCRCs.Sort();

	return FCrc::MemCrc32(ComponentsCRCs.GetData(), ComponentsCRCs.Num() * ComponentsCRCs.GetTypeSize());
}

namespace
{
	// Instance batcher class based on FISMComponentDescriptor
	struct FCustomISMComponentDescriptor : public FISMComponentDescriptor
	{
		FCustomISMComponentDescriptor(UStaticMeshComponent* SMC)
		{
			InitFrom(SMC, false);

			// We'll always want to spawn ISMC, even if our source components are all SMC
			ComponentClass = UInstancedStaticMeshComponent::StaticClass();

			// Stationnary can be considered as static for the purpose of HLODs
			if (Mobility == EComponentMobility::Stationary)
			{
				Mobility = EComponentMobility::Static;
			}

			ComputeHash();
		}
	};

	// Store batched instances data
	struct FInstancingData
	{
		int32							NumInstances = 0;

		TArray<FTransform>				InstancesTransforms;

		int32							NumCustomDataFloats = 0;
		TArray<float>					InstancesCustomData;

		TArray<FInstancedStaticMeshRandomSeed> RandomSeeds;
	};
}

TArray<UActorComponent*> UHLODBuilder::BatchInstances(const TArray<UActorComponent*>& InSourceComponents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODBuilderInstancing::Build);

	TArray<UStaticMeshComponent*> SourceStaticMeshComponents = FilterComponents<UStaticMeshComponent>(InSourceComponents);

	// Prepare instance batches
	TMap<FISMComponentDescriptor, FInstancingData> InstancesData;
	for (UStaticMeshComponent* SMC : SourceStaticMeshComponents)
	{
		FCustomISMComponentDescriptor ISMComponentDescriptor(SMC);
		FInstancingData& InstancingData = InstancesData.FindOrAdd(ISMComponentDescriptor);

		if (UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC))
		{
			InstancingData.NumCustomDataFloats = FMath::Max(InstancingData.NumCustomDataFloats, ISMC->NumCustomDataFloats);
			InstancingData.RandomSeeds.Add({ InstancingData.NumInstances, ISMC->InstancingRandomSeed });
			InstancingData.NumInstances += ISMC->GetInstanceCount();
		}
		else
		{
			InstancingData.NumCustomDataFloats = FMath::Max(InstancingData.NumCustomDataFloats, SMC->GetCustomPrimitiveData().Data.Num());
			InstancingData.NumInstances++;
		}
	}

	// Resize arrays
	for (auto& Entry : InstancesData)
	{
		const FISMComponentDescriptor& ISMComponentDescriptor = Entry.Key;
		FInstancingData& EntryInstancingData = Entry.Value;

		EntryInstancingData.InstancesTransforms.Reset(EntryInstancingData.NumInstances);
		EntryInstancingData.InstancesCustomData.Reset(EntryInstancingData.NumInstances * EntryInstancingData.NumCustomDataFloats);
	}

	// Append all transforms & per instance custom data
	for (UStaticMeshComponent* SMC : SourceStaticMeshComponents)
	{
		FCustomISMComponentDescriptor ISMComponentDescriptor(SMC);
		FInstancingData& InstancingData = InstancesData.FindChecked(ISMComponentDescriptor);

		if (UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC))
		{
			// Add transforms
			for (int32 InstanceIdx = 0; InstanceIdx < ISMC->GetInstanceCount(); InstanceIdx++)
			{
				FTransform& InstanceTransform = InstancingData.InstancesTransforms.AddDefaulted_GetRef();
				ISMC->GetInstanceTransform(InstanceIdx, InstanceTransform, true);
			}

			// Add per instance custom data
			int32 NumCustomDataFloatToAdd = ISMC->GetInstanceCount() * InstancingData.NumCustomDataFloats;
			InstancingData.InstancesCustomData.Append(ISMC->PerInstanceSMCustomData);
			InstancingData.InstancesCustomData.AddDefaulted(NumCustomDataFloatToAdd - ISMC->PerInstanceSMCustomData.Num());
		}
		else
		{
			// Add transform
			InstancingData.InstancesTransforms.Add(SMC->GetComponentTransform());

			// Add custom data
			InstancingData.InstancesCustomData.Append(SMC->GetCustomPrimitiveData().Data);
			InstancingData.InstancesCustomData.AddDefaulted(InstancingData.NumCustomDataFloats - SMC->GetCustomPrimitiveData().Data.Num());
		}
	}

	// Create an ISMC for each SM asset we found
	TArray<UActorComponent*> HLODComponents;
	for (auto& Entry : InstancesData)
	{
		const FISMComponentDescriptor& ISMComponentDescriptor = Entry.Key;
		FInstancingData& EntryInstancingData = Entry.Value;

		check(EntryInstancingData.InstancesTransforms.Num() * EntryInstancingData.NumCustomDataFloats == EntryInstancingData.InstancesCustomData.Num());

		UInstancedStaticMeshComponent* Component = ISMComponentDescriptor.CreateComponent(GetTransientPackage());
		Component->SetForcedLodModel(Component->GetStaticMesh()->GetNumLODs());
		Component->NumCustomDataFloats = EntryInstancingData.NumCustomDataFloats;
		Component->AddInstances(EntryInstancingData.InstancesTransforms, /*bShouldReturnIndices*/false, /*bWorldSpace*/true);
		Component->PerInstanceSMCustomData = MoveTemp(EntryInstancingData.InstancesCustomData);

		if (!EntryInstancingData.RandomSeeds.IsEmpty())
		{
			Component->InstancingRandomSeed = EntryInstancingData.RandomSeeds[0].RandomSeed;
		}

		if (EntryInstancingData.RandomSeeds.Num() > 1)
		{
			Component->AdditionalRandomSeeds = TArrayView<FInstancedStaticMeshRandomSeed>(&EntryInstancingData.RandomSeeds[1], EntryInstancingData.RandomSeeds.Num() - 1);
		}

		HLODComponents.Add(Component);
	};

	return HLODComponents;
}

static bool ShouldBatchComponent(UActorComponent* ActorComponent)
{
	bool bShouldBatch = false;

	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ActorComponent))
	{
		switch (PrimitiveComponent->HLODBatchingPolicy)
		{
		case EHLODBatchingPolicy::None:
			break;
		case EHLODBatchingPolicy::Instancing:
			bShouldBatch = true;
			break;
		case EHLODBatchingPolicy::MeshSection:
			bShouldBatch = true;
			UE_LOG(LogHLODBuilder, Warning, TEXT("EHLODBatchingPolicy::MeshSection is not yet supported by the HLOD builder, falling back to EHLODBatchingPolicy::Instancing."));
			break;
		default:
			checkNoEntry();
		}
	}

	return bShouldBatch;
}

TArray<UActorComponent*> UHLODBuilder::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<AActor*>& InSourceActors) const
{
	TArray<UActorComponent*> HLODRelevantComponents = GatherHLODRelevantComponents(InSourceActors);
	if (HLODRelevantComponents.IsEmpty())
	{
		return {};
	}

	// Handle components using a batching policy separately
	TArray<UActorComponent*> InputComponents;
	TArray<UActorComponent*> ComponentsToBatch;
	for (UActorComponent* SourceComponent : HLODRelevantComponents)
	{
		if (ShouldBatchComponent(SourceComponent))
		{
			ComponentsToBatch.Add(SourceComponent);
		}
		else
		{
			InputComponents.Add(SourceComponent);
		}
	}

	TMap<TSubclassOf<UHLODBuilder>, TArray<UActorComponent*>> HLODBuildersForComponents;

	// Gather custom HLOD builders, and regroup all components by builders
	for (UActorComponent* SourceComponent : InputComponents)
	{
		TSubclassOf<UHLODBuilder> HLODBuilderClass = SourceComponent->GetCustomHLODBuilderClass();
		HLODBuildersForComponents.FindOrAdd(HLODBuilderClass).Add(SourceComponent);
	}

	// If any of the builders requires it, wait for assets compilation to finish
	for (const auto& HLODBuilderPair : HLODBuildersForComponents)
	{
		const UHLODBuilder* HLODBuilder = HLODBuilderPair.Key ? HLODBuilderPair.Key->GetDefaultObject<UHLODBuilder>() : this;
		if (HLODBuilder->RequiresCompiledAssets())
		{
			FAssetCompilingManager::Get().FinishAllCompilation();
			break;
		}
	}	
	
	// Build HLOD components by sending source components to the individual builders, in batch
	TArray<UActorComponent*> HLODComponents;
	for (const auto& HLODBuilderPair : HLODBuildersForComponents)
	{
		// If no custom HLOD builder is provided, use this current builder.
		const UHLODBuilder* HLODBuilder = HLODBuilderPair.Key ? HLODBuilderPair.Key->GetDefaultObject<UHLODBuilder>() : this;
		const TArray<UActorComponent*>& SourceComponents = HLODBuilderPair.Value;

		TArray<UActorComponent*> NewComponents = HLODBuilder->Build(InHLODBuildContext, SourceComponents);
		HLODComponents.Append(NewComponents);
	}

	// Append batched components
	HLODComponents.Append(BatchInstances(ComponentsToBatch));

	// In case a builder returned null entries, clean the array.
	HLODComponents.RemoveSwap(nullptr);

	return HLODComponents;
}

#endif // WITH_EDITOR

