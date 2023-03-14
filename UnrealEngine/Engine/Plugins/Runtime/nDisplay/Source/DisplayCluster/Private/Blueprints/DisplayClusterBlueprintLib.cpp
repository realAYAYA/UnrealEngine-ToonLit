// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterBlueprintLib.h"
#include "Blueprints/DisplayClusterBlueprintAPIImpl.h"
#include "UObject/Package.h"

#include "DisplayClusterLightCardActor.h"
#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "UDisplayClusterBlueprintLib"

UDisplayClusterBlueprintLib::UDisplayClusterBlueprintLib(class FObjectInitializer const & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDisplayClusterBlueprintLib::GetAPI(TScriptInterface<IDisplayClusterBlueprintAPI>& OutAPI)
{
	static UDisplayClusterBlueprintAPIImpl* Obj = NewObject<UDisplayClusterBlueprintAPIImpl>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	OutAPI = Obj;
}

ADisplayClusterLightCardActor* UDisplayClusterBlueprintLib::CreateLightCard(ADisplayClusterRootActor* RootActor)
{
	if (!RootActor)
	{
		return nullptr;
	}

	// Create the light card
#if WITH_EDITOR
	FScopedTransaction Transaction(LOCTEXT("CreateLightCard", "Create Light Card"));
#endif

	const FVector SpawnLocation = RootActor->GetDefaultCamera()->GetComponentLocation();
	FRotator SpawnRotation = RootActor->GetDefaultCamera()->GetComponentRotation();
	SpawnRotation.Yaw -= 180.f;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.bNoFail = true;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParameters.Name = TEXT("LightCard");
	SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParameters.OverrideLevel = RootActor->GetLevel();

	ADisplayClusterLightCardActor* NewActor = CastChecked<ADisplayClusterLightCardActor>(
		RootActor->GetWorld()->SpawnActor(ADisplayClusterLightCardActor::StaticClass(),
			&SpawnLocation, &SpawnRotation, MoveTemp(SpawnParameters)));

	NewActor->Latitude = 30.0;
	NewActor->Longitude = 180.0;
	NewActor->Color = FLinearColor::Gray;

#if WITH_EDITOR
	NewActor->SetActorLabel(NewActor->GetName());
#endif

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepWorld, false);
	NewActor->AttachToActor(RootActor, AttachmentRules);

	// Add it to the root actor
	NewActor->AddToLightCardLayer(RootActor);

#if WITH_EDITOR
	// Required so operator panel updates
	RootActor->PostEditChange();
#endif
	
	return NewActor;
}

void UDisplayClusterBlueprintLib::DuplicateLightCards(TArray<ADisplayClusterLightCardActor*> OriginalLightcards, TArray<ADisplayClusterLightCardActor*>& OutNewLightCards)
{
#if WITH_EDITOR
	TMap<UWorld*, FCachedActorLabels> ActorLabelsByWorld;
	FScopedTransaction Transaction(LOCTEXT("DuplicateLightCard", "Duplicate Light Cards"));
#endif

	for (ADisplayClusterLightCardActor* OriginalLightcard : OriginalLightcards)
	{
		if (!OriginalLightcard)
		{
			continue;
		}

		ADisplayClusterRootActor* RootActor = OriginalLightcard->GetRootActorOwner().Get();
		if (!RootActor)
		{
			continue;
		}

		UWorld* World = RootActor->GetWorld();
		ULevel* Level = World ? World->GetCurrentLevel() : nullptr;
		if (!Level)
		{
			continue;
		}

		const FName UniqueName = MakeUniqueObjectName(Level, OriginalLightcard->GetClass());
		ADisplayClusterLightCardActor* NewLightCard = CastChecked<ADisplayClusterLightCardActor>(StaticDuplicateObject(OriginalLightcard, Level, UniqueName));

#if WITH_EDITOR
		Level->AddLoadedActor(NewLightCard);

		FCachedActorLabels* ActorLabels = ActorLabelsByWorld.Find(World);
		if (!ActorLabels)
		{
			ActorLabels = &ActorLabelsByWorld.Emplace(World, World);
		}

		FActorLabelUtilities::SetActorLabelUnique(NewLightCard, NewLightCard->GetActorLabel(), ActorLabels);
		ActorLabels->Add(NewLightCard->GetActorLabel());
#endif

		FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepWorld, false);
		NewLightCard->AttachToActor(RootActor, AttachmentRules);

		// Add it to the root actor
		NewLightCard->AddToLightCardLayer(RootActor);

#if WITH_EDITOR
		// Required so operator panel updates
		RootActor->PostEditChange();
#endif

#if WITH_EDITOR
		if (GIsEditor)
		{
			GEditor->BroadcastLevelActorAdded(NewLightCard);
		}
#endif

		OutNewLightCards.Add(NewLightCard);
	}
}

void UDisplayClusterBlueprintLib::FindLightCardsForRootActor(ADisplayClusterRootActor* RootActor, TSet<ADisplayClusterLightCardActor*>& OutLightCards)
{
	if (!RootActor)
	{
		return;
	}

	FDisplayClusterConfigurationICVFX_VisibilityList& RootActorLightCards = RootActor->GetConfigData()->StageSettings.Lightcard.ShowOnlyList;

	for (const TSoftObjectPtr<AActor>& LightCardActor : RootActorLightCards.Actors)
	{
		if (!LightCardActor.IsValid() || !LightCardActor->IsA<ADisplayClusterLightCardActor>())
		{
			continue;
		}

		OutLightCards.Add(Cast<ADisplayClusterLightCardActor>(LightCardActor.Get()));
	}

	// If there are any layers that are specified as light card layers, iterate over all actors in the world and 
	// add any that are members of any of the light card layers to the list. Only add an actor once, even if it is
	// in multiple layers
	if (RootActorLightCards.ActorLayers.IsEmpty())
	{
		return;
	}

	if (UWorld* World = RootActor->GetWorld())
	{
		for (TActorIterator<ADisplayClusterLightCardActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			if (!IsValid(*ActorIt))
			{
				continue;
			}

			for (const FActorLayer& ActorLayer : RootActorLightCards.ActorLayers)
			{
				if (ActorIt->Layers.Contains(ActorLayer.Name))
				{
					OutLightCards.Add(*ActorIt);
					break;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
