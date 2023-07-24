// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomBindingActions.h"

#include "ContentBrowserMenuContexts.h"
#include "GroomAsset.h"

#include "GeometryCache.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "ToolMenuSection.h"
#include "GroomBindingBuilder.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

uint32 FGroomBindingActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FGroomBindingActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GroomBinding", "GroomBinding");
}

UClass* FGroomBindingActions::GetSupportedClass() const
{
	return UGroomBindingAsset::StaticClass();
}

FColor FGroomBindingActions::GetTypeColor() const
{
	return FColor::White;
}

void FGroomBindingActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// #ueent_todo: Will need a custom editor at some point, for now just use the Properties editor
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FGroomBindingActions::RegisterMenus()
{
	FToolMenuOwnerScoped MenuOwner(UE_MODULE_NAME);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.GroomBinding");
	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

	Section.AddDynamicEntry("GroomBinding", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const TAttribute<FText> Label = LOCTEXT("RebuildGroomBinding", "Rebuild");
		const TAttribute<FText> ToolTip = LOCTEXT("RebuildGroomBindingTooltip", "Rebuild the groom binding");
		const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions");
		const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&FGroomBindingActions::ExecuteRebuildBindingAsset);

		InSection.AddMenuEntry("GroomBinding_Rebuild", Label, ToolTip, Icon, UIAction);
	}));
}

void FGroomBindingActions::ExecuteRebuildBindingAsset(const FToolMenuContext& MenuContext)
{
	if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
	{
		for (UGroomBindingAsset* BindingAsset : Context->LoadSelectedObjects<UGroomBindingAsset>())
		{
			if (BindingAsset->Groom && BindingAsset->HasValidTarget())
			{
				BindingAsset->Groom->ConditionalPostLoad();
				if (BindingAsset->GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
				{
					BindingAsset->TargetSkeletalMesh->ConditionalPostLoad();
					if (BindingAsset->SourceSkeletalMesh)
					{
						BindingAsset->SourceSkeletalMesh->ConditionalPostLoad();
					}
				}
				else
				{
					BindingAsset->TargetGeometryCache->ConditionalPostLoad();
					if (BindingAsset->SourceGeometryCache)
					{
						BindingAsset->SourceGeometryCache->ConditionalPostLoad();
					}
				}
				FGroomBindingBuilder::BuildBinding(BindingAsset, true);
				BindingAsset->GetOutermost()->MarkPackageDirty();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE