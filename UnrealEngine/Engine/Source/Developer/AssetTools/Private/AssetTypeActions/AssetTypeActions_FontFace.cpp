// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_FontFace.h"
#include "FontEditorModule.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "EditorReimportHandler.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_FontFace::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto FontFaces = GetTypedWeakObjectPtrs<UFontFace>(InObjects);

	Section.AddMenuEntry(
		"ReimportFontFaceLabel",
		LOCTEXT("ReimportFontFaceLabel", "Reimport"),
		LOCTEXT("ReimportFontFaceTooltip", "Reimport the selected font(s)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_FontFace::ExecuteReimport, FontFaces),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_FontFace::CanExecuteReimport, FontFaces)
			)
		);
}

void FAssetTypeActions_FontFace::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	// Load the FontEditor module to ensure that FFontFaceDetailsCustomization is registered
	IFontEditorModule* FontEditorModule = &FModuleManager::LoadModuleChecked<IFontEditorModule>("FontEditor");
	
	// Open each object in turn, as the default editor would try and collapse a multi-edit together 
	// into a single editor instance which doesn't really work for font face assets
	for (UObject* Object : InObjects)
	{
		FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Object);
	}
}

bool FAssetTypeActions_FontFace::CanExecuteReimport(const TArray<TWeakObjectPtr<UFontFace>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			// We allow a reimport if any of the fonts have a source filename
			if (!Object->SourceFilename.IsEmpty())
			{
				return true;
			}
		}
	}

	return false;
}

void FAssetTypeActions_FontFace::ExecuteReimport(const TArray<TWeakObjectPtr<UFontFace>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			// Skip fonts that don't have a source filename, as they can't be reimported
			if (!Object->SourceFilename.IsEmpty())
			{
				// Fonts fail to reimport if they ask for a new file if missing
				FReimportManager::Instance()->Reimport(Object, /*bAskForNewFileIfMissing=*/false);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
