// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"

#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayerInstanceWithAsset)

UDataLayerInstanceWithAsset::UDataLayerInstanceWithAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
FName UDataLayerInstanceWithAsset::MakeName(const UDataLayerAsset* DeprecatedDataLayer)
{
	return FName(FString::Format(TEXT("DataLayer_{0}"), { FGuid::NewGuid().ToString() }));
}

void UDataLayerInstanceWithAsset::OnCreated(const UDataLayerAsset* Asset)
{
	check(!GetOuterAWorldDataLayers()->HasDeprecatedDataLayers() || IsRunningCommandlet());

	Modify(/*bAlwaysMarkDirty*/false);

	check(DataLayerAsset == nullptr);
	DataLayerAsset = Asset;

	SetVisible(true);
}

bool UDataLayerInstanceWithAsset::IsReadOnly() const
{
	return GetOuterAWorldDataLayers()->IsSubWorldDataLayers();
}

bool UDataLayerInstanceWithAsset::IsLocked() const
{
	if (Super::IsLocked())
	{
		return true;
	}
	return IsReadOnly();
}

bool UDataLayerInstanceWithAsset::AddActor(AActor* Actor) const
{	
	check(GetWorld()->GetSubsystem<UDataLayerSubsystem>()->GetDataLayerInstance(DataLayerAsset) != nullptr);
	check(GetTypedOuter<ULevel>() == Actor->GetLevel()); // Make sure the instance is part of the same world as the actor.
	return Actor->AddDataLayer(DataLayerAsset);
}

bool UDataLayerInstanceWithAsset::RemoveActor(AActor* Actor) const
{
	return Actor->RemoveDataLayer(DataLayerAsset);
}

bool UDataLayerInstanceWithAsset::Validate(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	bool bIsValid = true;

	if (GetAsset() == nullptr)
	{
		ErrorHandler->OnInvalidReferenceDataLayerAsset(this);
		return false;
	}

	UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld());
	DataLayerSubsystem->ForEachDataLayer([&bIsValid, this, ErrorHandler](UDataLayerInstance* DataLayerInstance)
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
	}, GetOuterAWorldDataLayers()->GetLevel()); // Resolve DataLayerInstances based on outer level

	bIsValid &= Super::Validate(ErrorHandler);

	return bIsValid;
}
#endif
