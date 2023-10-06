// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "GameFramework/Actor.h"
#include "Engine/Level.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayerAsset)

UDataLayerAsset::UDataLayerAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DataLayerType(EDataLayerType::Editor)
	, DebugColor(FColor::Black)
{}

bool UDataLayerAsset::IsPrivate() const
{
	return !!GetTypedOuter<UDataLayerInstance>();
}

#if WITH_EDITOR
bool UDataLayerAsset::CanBeReferencedByActor(const TSoftObjectPtr<UDataLayerAsset>& InDataLayerAsset, AActor* InActor)
{
	if (UDataLayerAsset* DataLayerAsset = InDataLayerAsset.Get())
	{
		return DataLayerAsset->CanBeReferencedByActor(InActor);
	}

	// Only accept references to unloaded UDataLayerAsset if the path is indeed an Asset Path (no sub path string)
	return InDataLayerAsset.ToSoftObjectPath().IsAsset();
}

bool UDataLayerAsset::CanBeReferencedByActor(AActor* InActor) const
{
	return !IsPrivate() || GetTypedOuter<ULevel>() == InActor->GetLevel();
}

void UDataLayerAsset::PostLoad()
{
	if (DebugColor == FColor::Black)
	{
		DebugColor = FColor::MakeRandomSeededColor(GetTypeHash(GetName()));
	}

	Super::PostLoad();
}

bool UDataLayerAsset::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty)
	{
		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDataLayerAsset, DataLayerType))
		{
			// UDataLayerAsset outered to a UDataLayerInstance do not support Runtime type
			return !IsPrivate();
		}
	}

	return true;
}
#endif
