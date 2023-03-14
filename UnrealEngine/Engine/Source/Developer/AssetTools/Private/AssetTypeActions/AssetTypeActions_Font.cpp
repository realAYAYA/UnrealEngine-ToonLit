// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Font.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "EditorReimportHandler.h"
#include "FontEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Font::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Fonts = GetTypedWeakObjectPtrs<UFont>(InObjects);

	Section.AddMenuEntry(
		"ReimportFont",
		LOCTEXT("ReimportFontLabel", "Reimport"),
		LOCTEXT("ReimportFontTooltip", "Reimport the selected font(s)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_Font::ExecuteReimport, Fonts),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_Font::CanExecuteReimport, Fonts)
			)
		);
}

void FAssetTypeActions_Font::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Font = Cast<UFont>(*ObjIt);
		if (Font != NULL)
		{
			IFontEditorModule* FontEditorModule = &FModuleManager::LoadModuleChecked<IFontEditorModule>("FontEditor");
			FontEditorModule->CreateFontEditor(Mode, EditWithinLevelEditor, Font);
		}
	}
}

bool FAssetTypeActions_Font::CanExecuteReimport(const TArray<TWeakObjectPtr<UFont>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			// We allow a reimport if any of the fonts are using an offline cache
			if (Object->FontCacheType == EFontCacheType::Offline)
			{
				return true;
			}
		}
	}

	return false;
}

void FAssetTypeActions_Font::ExecuteReimport(const TArray<TWeakObjectPtr<UFont>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			// Skip fonts that aren't using an offline cache, as they can't be reimported
			if (Object->FontCacheType == EFontCacheType::Offline)
			{
				// Fonts fail to reimport if they ask for a new file if missing
				FReimportManager::Instance()->Reimport(Object, /*bAskForNewFileIfMissing=*/false);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
