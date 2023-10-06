// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerInstancePrivate.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "Engine/Level.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayerInstancePrivate)

UDataLayerInstancePrivate::UDataLayerInstancePrivate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, bIsIncludedInActorFilterDefault(true)
#endif
{
	DataLayerAsset = CreateDefaultSubobject<UDataLayerAsset>(TEXT("DataLayerAsset"));
#if WITH_EDITOR
	DataLayerAsset->SetType(EDataLayerType::Editor);
#endif
}

#if WITH_EDITOR
FName UDataLayerInstancePrivate::MakeName()
{
	return FName(FString::Format(TEXT("DataLayer_{0}"), { FGuid::NewGuid().ToString() }));
}

void UDataLayerInstancePrivate::OnCreated()
{
	check(!GetOuterWorldDataLayers()->HasDeprecatedDataLayers() || IsRunningCommandlet());

	Modify(/*bAlwaysMarkDirty*/false);
	FDataLayerUtils::SetDataLayerShortName(this, TEXT("DataLayer"));
		
	SetVisible(true);
}

bool UDataLayerInstancePrivate::IsReadOnly() const
{
	if (Super::IsReadOnly())
	{
		return true;
	}
	return GetOuterWorldDataLayers()->IsReadOnly();
}

bool UDataLayerInstancePrivate::IsLocked() const
{
	if (Super::IsLocked())
	{
		return true;
	}
	return IsReadOnly();
}

bool UDataLayerInstancePrivate::PerformAddActor(AActor* InActor) const
{
	check(GetOuterWorldDataLayers() == InActor->GetLevel()->GetWorldDataLayers()); // Make sure the instance is part of the same WorldDataLayers as the actor's level WorldDataLayer.
	check(UDataLayerManager::GetDataLayerManager(InActor)->GetDataLayerInstance(DataLayerAsset) != nullptr); // Make sure the DataLayerInstance exists for this level
	return FAssignActorDataLayer::AddDataLayerAsset(InActor, DataLayerAsset);
}

bool UDataLayerInstancePrivate::PerformRemoveActor(AActor* InActor) const
{
	return FAssignActorDataLayer::RemoveDataLayerAsset(InActor, DataLayerAsset);
}

bool UDataLayerInstancePrivate::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty && (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDataLayerInstancePrivate, bIsIncludedInActorFilterDefault)))
	{
		return SupportsActorFilters();
	}

	return true;
}

#endif
