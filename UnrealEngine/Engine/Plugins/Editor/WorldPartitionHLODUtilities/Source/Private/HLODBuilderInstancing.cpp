// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLODBuilderInstancing.h"
#include "Serialization/ArchiveCrc32.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODBuilderInstancing)


UHLODBuilderInstancingSettings::UHLODBuilderInstancingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bDisallowNanite(false)
{
}

uint32 UHLODBuilderInstancingSettings::GetCRC() const
{
	UHLODBuilderInstancingSettings& This = *const_cast<UHLODBuilderInstancingSettings*>(this);

	FArchiveCrc32 Ar;

	Ar << This.bDisallowNanite;
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - bDisallowNanite = %d"), Ar.GetCrc());

	return Ar.GetCrc();
}


UHLODBuilderInstancing::UHLODBuilderInstancing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSubclassOf<UHLODBuilderSettings> UHLODBuilderInstancing::GetSettingsClass() const
{
	return UHLODBuilderInstancingSettings::StaticClass();
}

TArray<UActorComponent*> UHLODBuilderInstancing::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	const UHLODBuilderInstancingSettings& InstancingSettings = *CastChecked<UHLODBuilderInstancingSettings>(HLODBuilderSettings);

	TArray<UActorComponent*> HLODComponents = UHLODBuilder::BatchInstances(InSourceComponents);

	// If requested, disallow Nanite on components whose mesh is Nanite enabled and has multiple LODs
	if (InstancingSettings.bDisallowNanite)
	{
		for (UActorComponent* HLODComponent : HLODComponents)
		{
			if (UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(HLODComponent))
			{
				if (UStaticMesh* StaticMesh = SMComponent->GetStaticMesh())
				{
					if (!SMComponent->bDisallowNanite && StaticMesh->HasValidNaniteData() && StaticMesh->GetNumLODs() > 1)
					{
						SMComponent->bDisallowNanite = true;
						SMComponent->MarkRenderStateDirty();
					}
				}
			}
		}
	}

	return HLODComponents;
}

