// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorPlugin.h"
#include "Dataflow/DataflowEditorStyle.h"

#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "DataflowEditorToolkit.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowAssetActions.h"
#include "Dataflow/DataflowSNodeFactories.h"

#define LOCTEXT_NAMESPACE "DataflowEditor"

//#define BOX_BRUSH(StyleSet, RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
//#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

void IDataflowEditorPlugin::StartupModule()
{
	FDataflowEditorStyle::Get();

	DataflowAssetActions = new FDataflowAssetActions();

	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	AssetTools.RegisterAssetTypeActions(MakeShareable(DataflowAssetActions));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	DataflowSNodeFactory = MakeShareable(new FDataflowSNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(DataflowSNodeFactory);
}

void IDataflowEditorPlugin::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();

		AssetTools.UnregisterAssetTypeActions(DataflowAssetActions->AsShared());

		FEdGraphUtilities::UnregisterVisualNodeFactory(DataflowSNodeFactory);
	}
}

TSharedRef<FAssetEditorToolkit> IDataflowEditorPlugin::CreateDataflowAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* FleshAsset)
{
	TSharedPtr<FDataflowEditorToolkit> NewDataflowAssetEditor = MakeShared<FDataflowEditorToolkit>();
	NewDataflowAssetEditor->InitDataflowEditor(Mode, InitToolkitHost, FleshAsset);
	return StaticCastSharedPtr<FAssetEditorToolkit>(NewDataflowAssetEditor).ToSharedRef();
}


IMPLEMENT_MODULE(IDataflowEditorPlugin, DataflowEditor)


#undef LOCTEXT_NAMESPACE
