// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "Engine/Level.h"
#include "Misc/StringFormatArg.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayerInstanceWithAsset)

UDataLayerInstanceWithAsset::UDataLayerInstanceWithAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, bIsIncludedInActorFilterDefault(true)
#endif
{

}

#if WITH_EDITOR
FName UDataLayerInstanceWithAsset::MakeName(const UDataLayerAsset* DeprecatedDataLayer)
{
	return FName(FString::Format(TEXT("DataLayer_{0}"), { FGuid::NewGuid().ToString() }));
}

TSubclassOf<UDataLayerInstanceWithAsset> UDataLayerInstanceWithAsset::GetDataLayerInstanceClass()
{
	return UDataLayerManager::GetDataLayerInstanceWithAssetClass();
}

void UDataLayerInstanceWithAsset::OnCreated(const UDataLayerAsset* Asset)
{
	check(!GetOuterWorldDataLayers()->HasDeprecatedDataLayers() || IsRunningCommandlet());

	Modify(/*bAlwaysMarkDirty*/false);

	check(DataLayerAsset == nullptr);
	DataLayerAsset = Asset;

	SetVisible(true);
}

bool UDataLayerInstanceWithAsset::IsReadOnly() const
{
	if (Super::IsReadOnly())
	{
		return true;
	}
	return GetOuterWorldDataLayers()->IsReadOnly();
}

bool UDataLayerInstanceWithAsset::IsLocked() const
{
	if (Super::IsLocked())
	{
		return true;
	}
	return IsReadOnly();
}

bool UDataLayerInstanceWithAsset::CanAddActor(AActor* InActor) const
{
	return DataLayerAsset != nullptr && DataLayerAsset->CanBeReferencedByActor(InActor) && Super::CanAddActor(InActor);
}

bool UDataLayerInstanceWithAsset::PerformAddActor(AActor* InActor) const
{	
	check(GetOuterWorldDataLayers() == InActor->GetLevel()->GetWorldDataLayers()); // Make sure the instance is part of the same WorldDataLayers as the actor's level WorldDataLayer.
	check(UDataLayerManager::GetDataLayerManager(InActor)->GetDataLayerInstance(DataLayerAsset) != nullptr); // Make sure the DataLayerInstance exists for this level
	return FAssignActorDataLayer::AddDataLayerAsset(InActor, DataLayerAsset);
}

bool UDataLayerInstanceWithAsset::CanRemoveActor(AActor* InActor) const
{
	return DataLayerAsset != nullptr && Super::CanRemoveActor(InActor);
}

bool UDataLayerInstanceWithAsset::PerformRemoveActor(AActor* InActor) const
{
	return FAssignActorDataLayer::RemoveDataLayerAsset(InActor, DataLayerAsset);
}

bool UDataLayerInstanceWithAsset::Validate(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	bool bIsValid = true;

	if (GetAsset() == nullptr)
	{
		ErrorHandler->OnInvalidReferenceDataLayerAsset(this);
		return false;
	}

	// Get the DataLayerManager for this DataLayerInstance which will be the one of its outer world
	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(this);
	if (ensure(DataLayerManager))
	{
		DataLayerManager->ForEachDataLayerInstance([&bIsValid, this, ErrorHandler](UDataLayerInstance* DataLayerInstance)
		{
			if (DataLayerInstance != this)
			{
				if (UDataLayerInstanceWithAsset* DataLayerInstanceWithAsset = Cast<UDataLayerInstanceWithAsset>(DataLayerInstance))
				{
					if (DataLayerInstanceWithAsset->GetAsset() == GetAsset())
					{
						ErrorHandler->OnDataLayerAssetConflict(this, DataLayerInstanceWithAsset);
						bIsValid = false;
						return false;
					}
				}
			}
	
			return true;
		});
	}

	bIsValid &= Super::Validate(ErrorHandler);

	return bIsValid;
}

void UDataLayerInstanceWithAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataLayerInstanceWithAsset, DataLayerAsset))
	{
		GetOuterWorldDataLayers()->ResolveActorDescContainers();
	}
}

bool UDataLayerInstanceWithAsset::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty && (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDataLayerInstanceWithAsset, bIsIncludedInActorFilterDefault)))
	{
		return SupportsActorFilters();
	}

	return true;
}

bool UDataLayerInstanceWithAsset::SupportsActorFilters() const
{
	return DataLayerAsset && DataLayerAsset->SupportsActorFilters();
}

bool UDataLayerInstanceWithAsset::IsIncludedInActorFilterDefault() const
{
	return bIsIncludedInActorFilterDefault;
}

void UDataLayerInstanceWithAsset::PreEditUndo()
{
	Super::PreEditUndo();
	CachedDataLayerAsset = DataLayerAsset;
}

void UDataLayerInstanceWithAsset::PostEditUndo()
{
	Super::PostEditUndo();
	if (CachedDataLayerAsset != DataLayerAsset)
	{
		GetOuterWorldDataLayers()->ResolveActorDescContainers();
	}
	CachedDataLayerAsset = nullptr;
}
#endif
