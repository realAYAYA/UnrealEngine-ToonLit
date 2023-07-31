// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomActorFactory.h"
#include "GroomAsset.h"
#include "GroomActor.h"
#include "GroomComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomActorFactory)

#define LOCTEXT_NAMESPACE "GroomActorFactory"

UGroomActorFactory::UGroomActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DefaultName", "Groom");
	NewActorClass = AGroomActor::StaticClass();
	bUseSurfaceOrientation = false;
	bShowInEditorQuickMenu = true;
}

bool UGroomActorFactory::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.IsInstanceOf( UGroomAsset::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoGroomAsset", "A valid groom asset must be specified.");
		return false;
	}

	return true;
}

void UGroomActorFactory::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UGroomAsset* GroomAsset = CastChecked<UGroomAsset>(Asset);

	// Change properties
	AGroomActor* GroomActor = CastChecked<AGroomActor>( NewActor );
	UGroomComponent* GroomComponent = GroomActor->GetGroomComponent();
	check(GroomComponent);

	GroomComponent->UnregisterComponent();
	GroomComponent->SetGroomAsset(GroomAsset);
	GroomComponent->PostLoad();
	GroomComponent->RegisterComponent();
}

UObject* UGroomActorFactory::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AGroomActor* SMA = CastChecked<AGroomActor>(Instance);

	check(SMA->GetGroomComponent());
	return SMA->GetGroomComponent()->GroomAsset;
}

void UGroomActorFactory::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		UGroomAsset* GroomAsset = CastChecked<UGroomAsset>(Asset);
		AGroomActor* GroomActor = CastChecked<AGroomActor>(CDO);
		UGroomComponent* GroomComponent = GroomActor->GetGroomComponent();

		GroomComponent->SetGroomAsset(GroomAsset);
	}
}

FQuat UGroomActorFactory::AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const
{
	return FindActorAlignmentRotation(ActorRotation, FVector(0.f, 0.f, 1.f), InSurfaceNormal);
}


#undef LOCTEXT_NAMESPACE 
