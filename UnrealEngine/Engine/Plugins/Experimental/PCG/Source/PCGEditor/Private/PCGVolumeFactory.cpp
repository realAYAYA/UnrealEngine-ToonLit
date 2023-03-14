// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGVolumeFactory.h"

#include "PCGComponent.h"
#include "PCGEditorSettings.h"
#include "PCGGraph.h"
#include "PCGVolume.h"

#define LOCTEXT_NAMESPACE "PCGVolumeFactory"

UPCGVolumeFactory::UPCGVolumeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("PCGVolumeDisplayName", "PCG Volume");
	NewActorClass = APCGVolume::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UPCGVolumeFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if ( !AssetData.IsValid() || !AssetData.IsInstanceOf(UPCGGraph::StaticClass()))
	{
		OutErrorMsg = LOCTEXT("NoPCGGraph", "A valid PCG graph asset must be specified.");
		return false;
	}

	return true;
}

void UPCGVolumeFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	if (UPCGGraph* PCGGraph = Cast<UPCGGraph>(Asset))
	{
		const UPCGEditorSettings* PCGEditorSettings = GetDefault<UPCGEditorSettings>();
		
		APCGVolume* PCGVolume = CastChecked<APCGVolume>(NewActor);
		PCGVolume->SetActorScale3D(PCGEditorSettings->VolumeScale);
		
		UPCGComponent* PCGComponent = CastChecked<UPCGComponent>(PCGVolume->GetComponentByClass(UPCGComponent::StaticClass()));
		PCGComponent->SetGraph(PCGGraph);

		if (PCGEditorSettings->bGenerateOnDrop)
		{
			PCGComponent->Generate();
		}
	}
}

#undef LOCTEXT_NAMESPACE
