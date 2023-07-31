// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DataprepAsset.h"

#include "DataprepAsset.h"
#include "DataprepAssetInstance.h"
#include "DataprepEditor.h"

FText FAssetTypeActions_DataprepAsset::GetName() const
{
	return NSLOCTEXT("AssetTypeActions_DataprepAsset", "Name", "Dataprep Asset");
}

UClass* FAssetTypeActions_DataprepAsset::GetSupportedClass() const
{
	return UDataprepAsset::StaticClass();
}

void FAssetTypeActions_DataprepAsset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	if (InObjects.Num() == 0)
	{
		return;
	}

	for (UObject* Object : InObjects)
	{
		if (UDataprepAsset* DataprepAsset = Cast<UDataprepAsset>(Object))
		{
			TSharedRef<FDataprepEditor> NewDataprepEditor(new FDataprepEditor());
			NewDataprepEditor->InitDataprepEditor( EToolkitMode::Standalone, EditWithinLevelEditor, DataprepAsset );
		}
	}
}

FText FAssetTypeActions_DataprepAssetInstance::GetName() const
{
	return NSLOCTEXT("AssetTypeActions_DataprepAssetInstance", "Name", "Dataprep Asset Instance");
}

UClass* FAssetTypeActions_DataprepAssetInstance::GetSupportedClass() const
{
	return UDataprepAssetInstance::StaticClass();
}

void FAssetTypeActions_DataprepAssetInstance::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	if (InObjects.Num() == 0)
	{
		return;
	}

	for (UObject* Object : InObjects)
	{
		if (UDataprepAssetInstance* DataprepAssetInstance = Cast<UDataprepAssetInstance>(Object))
		{
			TSharedRef<FDataprepEditor> NewDataprepEditor(new FDataprepEditor());
			NewDataprepEditor->InitDataprepEditor( EToolkitMode::Standalone, EditWithinLevelEditor, DataprepAssetInstance );
		}
	}
}
