// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MediaBundle.h"

#include "AssetEditor/MediaBundleEditorToolkit.h"
#include "MediaBundle.h"


#define LOCTEXT_NAMESPACE "MediaBundleEditor"

FText FAssetTypeActions_MediaBundle::GetName() const
{
	return LOCTEXT("AssetTypeActions_MediaBundle", "Media Bundle");
}

UClass* FAssetTypeActions_MediaBundle::GetSupportedClass() const
{
	return UMediaBundle::StaticClass();
}

void FAssetTypeActions_MediaBundle::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Obj : InObjects)
	{
		if (UMediaBundle* Asset = Cast<UMediaBundle>(Obj))
		{
			FMediaBundleEditorToolkit::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Asset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
