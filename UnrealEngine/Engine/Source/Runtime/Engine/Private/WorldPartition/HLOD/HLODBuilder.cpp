// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/HLODProxy.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "HLOD/HLODBatchingPolicy.h"
#include "ISMPartition/ISMComponentBatcher.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"
#include "WorldPartition/HLOD/HLODTemplatedInstancedStaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODBuilder)


DEFINE_LOG_CATEGORY(LogHLODBuilder);


UHLODBuilder::UHLODBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, HLODInstancedStaticMeshComponentClass(UInstancedStaticMeshComponent::StaticClass())
#endif
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

bool UHLODBuilder::RequiresWarmup() const
{
	return true;
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
			UMaterialInterface* NaniteOverride = MaterialInterface ? MaterialInterface->GetNaniteOverride() : nullptr;
			if (NaniteOverride)
			{
				ComponentCRC = UHLODProxy::GetCRC(NaniteOverride, ComponentCRC);
				UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - Material (%s) = %x"), *NaniteOverride->GetName(), ComponentCRC);

				TArray<UTexture*> Textures;
				NaniteOverride->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
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

uint32 UHLODBuilder::ComputeHLODHash(const TArray<UActorComponent*>& InSourceComponents)
{
	// We get the CRC of each component
	TArray<uint32> ComponentsCRCs;

	for (UActorComponent* SourceComponent : InSourceComponents)
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

TSubclassOf<UInstancedStaticMeshComponent> UHLODBuilder::GetInstancedStaticMeshComponentClass()
{
	TSubclassOf<UInstancedStaticMeshComponent> ISMClass = StaticClass()->GetDefaultObject<UHLODBuilder>()->HLODInstancedStaticMeshComponentClass;
	if (!ISMClass)
	{
		FString ConfigValue;
		GConfig->GetString(TEXT("/Script/Engine.HLODBuilder"), TEXT("HLODInstancedStaticMeshComponentClass"), ConfigValue, GEditorIni);
		UE_LOG(LogHLODBuilder, Error, TEXT("Could not resolve the class specified for HLODInstancedStaticMeshComponentClass. Config value was %s"), *ConfigValue);

		// Fallback to standard ISMC
		ISMClass = UInstancedStaticMeshComponent::StaticClass();
	}
	return ISMClass;
}

namespace
{
	// Instance batcher class based on FISMComponentDescriptor
	struct FCustomISMComponentDescriptor : public FISMComponentDescriptor
	{
		TSubclassOf<AActor> TemplateActorClass;
		FName				TemplateComponentName;

		FCustomISMComponentDescriptor(UStaticMeshComponent* SMC)
		{
			InitFrom(SMC, false);

			// We'll always want to spawn ISMC, even if our source components are all SMC
			ComponentClass = UHLODBuilder::GetInstancedStaticMeshComponentClass();

			// Extra work for templated ISMC
			// This code should be reorganized
			if (ComponentClass->IsChildOf<UHLODTemplatedInstancedStaticMeshComponent>())
			{
				TemplateActorClass = SMC->GetOwner()->GetClass();
				TemplateComponentName = SMC->GetFName();
			}			

			// Stationnary can be considered as static for the purpose of HLODs
			if (Mobility == EComponentMobility::Stationary)
			{
				Mobility = EComponentMobility::Static;
			}

			ComputeHash();

			// Extra work for templated ISMC
			// This code should be reorganized
			if (ComponentClass->IsChildOf<UHLODTemplatedInstancedStaticMeshComponent>())
			{
				Hash = HashCombine(Hash, GetTypeHash(TemplateActorClass));
				Hash = HashCombine(Hash, GetTypeHash(TemplateComponentName));
			}
		}
	};
}

TArray<UActorComponent*> UHLODBuilder::BatchInstances(const TArray<UActorComponent*>& InSourceComponents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODBuilderInstancing::Build);

	TArray<UStaticMeshComponent*> SourceStaticMeshComponents = FilterComponents<UStaticMeshComponent>(InSourceComponents);

	// Prepare instance batches
	TMap<FCustomISMComponentDescriptor, FISMComponentBatcher> InstancesData;
	for (UStaticMeshComponent* SMC : SourceStaticMeshComponents)
	{
		FCustomISMComponentDescriptor ISMComponentDescriptor(SMC);
		FISMComponentBatcher& ISMComponentBatcher = InstancesData.FindOrAdd(ISMComponentDescriptor);
		ISMComponentBatcher.Add(SMC);
	}

	// Create an ISMC for each SM asset we found
	TArray<UActorComponent*> HLODComponents;
	for (auto& Entry : InstancesData)
	{
		const FCustomISMComponentDescriptor& ISMComponentDescriptor = Entry.Key;
		const FISMComponentBatcher& ISMComponentBatcher = Entry.Value;

		UInstancedStaticMeshComponent* ISMComponent = ISMComponentDescriptor.CreateComponent(GetTransientPackage());
		ISMComponentBatcher.InitComponent(ISMComponent);
		
		// Extra work for templated ISMC
		// This code should be reorganized
		if (UHLODTemplatedInstancedStaticMeshComponent* TemplatedISMC = Cast<UHLODTemplatedInstancedStaticMeshComponent>(ISMComponent))
		{
			TemplatedISMC->SetTemplateActorClass(ISMComponentDescriptor.TemplateActorClass);
			TemplatedISMC->SetTemplateComponentName(ISMComponentDescriptor.TemplateComponentName);
		}

		ISMComponent->SetForcedLodModel(ISMComponent->GetStaticMesh()->GetNumLODs());

		HLODComponents.Add(ISMComponent);
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
		{
			bShouldBatch = true;
			FString LogDetails = FString::Printf(TEXT("%s %s (from actor %s)"), *PrimitiveComponent->GetClass()->GetName(), *ActorComponent->GetName(), *ActorComponent->GetOwner()->GetActorLabel());
			if (UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
			{
				LogDetails += FString::Printf(TEXT(" using static mesh %s"), SMComponent->GetStaticMesh() ? *SMComponent->GetStaticMesh()->GetName() : TEXT("<null>"));
			}
			UE_LOG(LogHLODBuilder, Display, TEXT("EHLODBatchingPolicy::MeshSection is not yet supported by the HLOD builder, falling back to EHLODBatchingPolicy::Instancing for %s."), *LogDetails);
			break;
		}
		default:
			checkNoEntry();
		}
	}

	return bShouldBatch;
}

TArray<UActorComponent*> UHLODBuilder::Build(const FHLODBuildContext& InHLODBuildContext) const
{
	// Handle components using a batching policy separately
	TArray<UActorComponent*> InputComponents;
	TArray<UActorComponent*> ComponentsToBatch;

	if (!ShouldIgnoreBatchingPolicy())
	{				
		for (UActorComponent* SourceComponent : InHLODBuildContext.SourceComponents)
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
	}
	else
	{
		InputComponents = InHLODBuildContext.SourceComponents;
	}

	TMap<TSubclassOf<UHLODBuilder>, TArray<UActorComponent*>> HLODBuildersForComponents;

	// Gather custom HLOD builders, and regroup all components by builders
	for (UActorComponent* SourceComponent : InputComponents)
	{
		TSubclassOf<UHLODBuilder> HLODBuilderClass = SourceComponent->GetCustomHLODBuilderClass();
		HLODBuildersForComponents.FindOrAdd(HLODBuilderClass).Add(SourceComponent);
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

