// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MaterialInstanceConstant.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "AssetTools.h"
#include "MaterialEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_MaterialInstanceConstant::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto MICs = GetTypedWeakObjectPtrs<UMaterialInstanceConstant>(InObjects);

	FAssetTypeActions_MaterialInterface::GetActions(InObjects, Section);

	Section.AddMenuEntry(
		"MaterialInstanceConstant_FindParent",
		LOCTEXT("MaterialInstanceConstant_FindParent", "Find Parent"),
		LOCTEXT("MaterialInstanceConstant_FindParentTooltip", "Finds the material this instance is based on in the content browser."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.GenericFind"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_MaterialInstanceConstant::ExecuteFindParent, MICs ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_MaterialInstanceConstant::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MIC = Cast<UMaterialInstanceConstant>(*ObjIt);
		if (MIC != NULL)
		{
			IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
			MaterialEditorModule->CreateMaterialInstanceEditor(Mode, EditWithinLevelEditor, MIC);
		}
	}
}

void FAssetTypeActions_MaterialInstanceConstant::ExecuteFindParent(TArray<TWeakObjectPtr<UMaterialInstanceConstant>> Objects)
{
	TArray<UObject*> ObjectsToSyncTo;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			ObjectsToSyncTo.AddUnique( Object->Parent );
		}
	}

	// Sync the respective browser to the valid parents
	if ( ObjectsToSyncTo.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSyncTo);
	}
}

#undef LOCTEXT_NAMESPACE
