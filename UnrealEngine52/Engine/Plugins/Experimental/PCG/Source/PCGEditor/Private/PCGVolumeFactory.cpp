// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGVolumeFactory.h"

#include "PCGComponent.h"
#include "PCGEngineSettings.h"
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
	if (UActorFactory::CanCreateActorFrom(AssetData, OutErrorMsg))
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.IsInstanceOf<UPCGGraphInterface>())
	{
		OutErrorMsg = LOCTEXT("NoPCGGraph", "A valid PCG graph asset must be specified.");
		return false;
	}

	return true;
}

void UPCGVolumeFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	// Disable auto-refreshing on preview actors until we have something more robust on the execution side.
	if (NewActor && NewActor->bIsEditorPreviewActor)
	{
		return;
	}

	if (UPCGGraphInterface* PCGGraph = Cast<UPCGGraphInterface>(Asset))
	{
		const UPCGEngineSettings* Settings = GetDefault<UPCGEngineSettings>();
		
		APCGVolume* PCGVolume = CastChecked<APCGVolume>(NewActor);
		PCGVolume->SetActorScale3D(Settings->VolumeScale);
		
		UPCGComponent* PCGComponent = CastChecked<UPCGComponent>(PCGVolume->GetComponentByClass(UPCGComponent::StaticClass()));
		PCGComponent->SetGraph(PCGGraph);

		if (Settings->bGenerateOnDrop)
		{
			PCGComponent->Generate();
		}
	}
}

#undef LOCTEXT_NAMESPACE
