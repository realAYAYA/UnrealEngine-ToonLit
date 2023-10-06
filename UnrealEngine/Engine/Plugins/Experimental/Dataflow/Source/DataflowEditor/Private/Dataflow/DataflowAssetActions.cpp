// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowAssetActions.h"
#include "Dataflow/DataflowEditorPlugin.h"

#include "Dataflow/DataflowObject.h"

#define LOCTEXT_NAMESPACE "AssetActions_DataflowAsset"

FText FDataflowAssetActions::GetName() const
{
	return LOCTEXT("Name", "Dataflow Graph");
}

UClass* FDataflowAssetActions::GetSupportedClass() const
{
	return UDataflow::StaticClass();
}

FColor FDataflowAssetActions::GetTypeColor() const
{
	return FColor(255, 127, 40);
}

void FDataflowAssetActions::GetActions(const TArray<UObject*>& InObjects,
										class FMenuBuilder&     MenuBuilder)
{
}

void FDataflowAssetActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	//FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);

	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = Cast<UDataflow>(*ObjIt))
		{
			IDataflowEditorPlugin* DataflowEditorPlugin = &FModuleManager::LoadModuleChecked<IDataflowEditorPlugin>("DataflowEditor");
			DataflowEditorPlugin->CreateDataflowAssetEditor(Mode, EditWithinLevelEditor, Object);
		}
	}
}


uint32 FDataflowAssetActions::GetCategories()
{
	return EAssetTypeCategories::Physics;
}

FText FDataflowAssetActions::GetAssetDescription(const struct FAssetData& AssetData) const
{
	return LOCTEXT("Description", "A dataflow graph for asset authoring.");
}

#undef LOCTEXT_NAMESPACE
