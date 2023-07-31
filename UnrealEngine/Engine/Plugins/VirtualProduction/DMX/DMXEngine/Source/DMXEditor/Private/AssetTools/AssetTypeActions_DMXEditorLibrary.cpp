// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/AssetTypeActions_DMXEditorLibrary.h"
#include "Library/DMXLibrary.h"
#include "DMXEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_DMXEditorLibrary"

FText FAssetTypeActions_DMXEditorLibrary::GetName() const
{
	return LOCTEXT("DMXEditorLibrary", "DMX Library");
}

UClass * FAssetTypeActions_DMXEditorLibrary::GetSupportedClass() const
{
	return UDMXLibrary::StaticClass();
}

void FAssetTypeActions_DMXEditorLibrary::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	TArray<UDMXLibrary*> LibrariesToOpen;
	for (UObject* Obj : InObjects)
	{
		UDMXLibrary* Library = Cast<UDMXLibrary>(Obj);
		if (Library)
		{
			LibrariesToOpen.Add(Library);
		}
	}

	FDMXEditorModule& DMXEditorModule = FDMXEditorModule::Get();
	for (UDMXLibrary* Library : LibrariesToOpen)
	{
		DMXEditorModule.CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Library);
	}
}

#undef LOCTEXT_NAMESPACE
