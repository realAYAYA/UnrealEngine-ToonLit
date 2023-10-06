// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGVolumeFactory.h"

#include "PCGComponent.h"
#include "PCGEngineSettings.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "PCGVolume.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "Subsystems/PlacementSubsystem.h"

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

void UPCGVolumeFactory::PostPlaceAsset(TArrayView<const FTypedElementHandle> InElementHandles, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	Super::PostPlaceAsset(InElementHandles, InPlacementInfo, InPlacementOptions);

	// Preview elements are created while dragging an asset, but before dropping.
	if (InPlacementOptions.bIsCreatingPreviewElements)
	{
		return;
	}

	for (const FTypedElementHandle& PlacedElement : InElementHandles)
	{
		TTypedElement<ITypedElementObjectInterface> ObjectInterface = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementObjectInterface>(PlacedElement);
		AActor* NewActor = ObjectInterface ? ObjectInterface.GetObjectAs<AActor>() : nullptr;
		if (!NewActor)
		{
			continue;
		}

		UPCGGraphInterface* PCGGraph = Cast<UPCGGraphInterface>(InPlacementInfo.AssetToPlace.GetAsset());
		const UPCGEngineSettings* Settings = GetDefault<UPCGEngineSettings>();
		if (!PCGGraph || !Settings)
		{
			continue;
		}

		APCGVolume* PCGVolume = CastChecked<APCGVolume>(NewActor);
		PCGVolume->SetActorScale3D(Settings->VolumeScale);

		UPCGComponent* PCGComponent = CastChecked<UPCGComponent>(PCGVolume->GetComponentByClass(UPCGComponent::StaticClass()));
		PCGComponent->SetGraph(PCGGraph);

		if (Settings->bGenerateOnDrop)
		{
			if (UPCGSubsystem* Subsystem = PCGComponent->GetSubsystem())
			{
				TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);

				// Schedule a task to generate this component.
				// We cannot generate the component right away because after PostSpawnActor is called, all
				// actor components are unregistered and re-registered, which cancels component generation
				Subsystem->ScheduleGeneric([ComponentPtr]()
				{
					if (UPCGComponent* Component = ComponentPtr.Get())
					{
						// If the component is not valid anymore, just early out.
						if (!IsValid(Component))
						{
							return true;
						}

						Component->Generate();
					}

					return true;
				}, PCGComponent, /*TaskDependencies=*/{});
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
