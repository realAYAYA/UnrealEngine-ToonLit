// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGridBlueprintActions.h"
#include "Blueprints/RenderGridBlueprint.h"
#include "Factories/RenderGridFactory.h"
#include "RenderGridEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


FText UE::RenderGrid::Private::FRenderGridBlueprintActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_RenderGrid", "Render Grid");
}

FColor UE::RenderGrid::Private::FRenderGridBlueprintActions::GetTypeColor() const
{
	return FColor(200, 80, 80);
}

UClass* UE::RenderGrid::Private::FRenderGridBlueprintActions::GetSupportedClass() const
{
	return URenderGridBlueprint::StaticClass();
}

void UE::RenderGrid::Private::FRenderGridBlueprintActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (URenderGridBlueprint* RenderGridBlueprint = Cast<URenderGridBlueprint>(*ObjIt))
		{
			constexpr bool bBringToFrontIfOpen = true;
			if (IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(RenderGridBlueprint, bBringToFrontIfOpen))
			{
				EditorInstance->FocusWindow(RenderGridBlueprint);
			}
			else
			{
				const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
				IRenderGridEditorModule::Get().CreateRenderGridEditor(Mode, EditWithinLevelEditor, RenderGridBlueprint);
			}
		}
	}
}

uint32 UE::RenderGrid::Private::FRenderGridBlueprintActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

UFactory* UE::RenderGrid::Private::FRenderGridBlueprintActions::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	URenderGridBlueprintFactory* RenderGridBlueprintFactory = NewObject<URenderGridBlueprintFactory>();
	RenderGridBlueprintFactory->ParentClass = TSubclassOf<URenderGrid>(*InBlueprint->GeneratedClass);
	return RenderGridBlueprintFactory;
}


#undef LOCTEXT_NAMESPACE
