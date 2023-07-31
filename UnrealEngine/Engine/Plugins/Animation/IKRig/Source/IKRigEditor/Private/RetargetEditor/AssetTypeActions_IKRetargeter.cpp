// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/AssetTypeActions_IKRetargeter.h"

#include "Animation/AnimationAsset.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RetargetEditor/IKRetargetEditorStyle.h"
#include "RetargetEditor/SRetargetAnimAssetsWindow.h"
#include "Retargeter/IKRetargeter.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_IKRetargeter::GetSupportedClass() const
{
	return UIKRetargeter::StaticClass();
}

void FAssetTypeActions_IKRetargeter::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
    
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UIKRetargeter* Asset = Cast<UIKRetargeter>(*ObjIt))
		{
			TSharedRef<FIKRetargetEditor> NewEditor(new FIKRetargetEditor());
			NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, Asset);
		}
	}
}

UThumbnailInfo* FAssetTypeActions_IKRetargeter::GetThumbnailInfo(UObject* Asset) const
{
	UIKRetargeter* IKRetargeter = CastChecked<UIKRetargeter>(Asset);
	return NewObject<USceneThumbnailInfo>(IKRetargeter, NAME_None, RF_Transactional);
}

void FAssetTypeActions_IKRetargeter::ExtendAnimSequenceToolMenu()
{
	static const TArray<FName> MenusToExtend 
	{
		"ContentBrowser.AssetContextMenu.AnimSequence",
		"ContentBrowser.AssetContextMenu.BlendSpace",
		"ContentBrowser.AssetContextMenu.AimOffsetBlendSpace",
		"ContentBrowser.AssetContextMenu.BlendSpace1D",
		"ContentBrowser.AssetContextMenu.PoseAsset",
		"ContentBrowser.AssetContextMenu.AnimBlueprint",
		"ContentBrowser.AssetContextMenu.AnimMontage"
	};

	for (const FName& MenuName : MenusToExtend)
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
		check(Menu);

		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddSubMenu(
		"IKRetargetAnimSubmenu", 
		LOCTEXT("IKRetargetAnimSubmenu", "Retarget Animation Assets"),
		LOCTEXT("IKRetargetAnimSubmenu_ToolTip", "Opens the batch retargeting menu."),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* AlignmentMenu)
		{
			FToolMenuSection& InSection = AlignmentMenu->AddSection("IKRetargetMenu", LOCTEXT("RetargetHeader", "IK Retargeting"));
			InSection.AddDynamicEntry("IKRigActions", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				FAssetTypeActions_IKRetargeter::CreateRetargetSubMenu(InSection);
			}));
		}),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.RetargetManager")
		);
	}
}

void FAssetTypeActions_IKRetargeter::CreateRetargetSubMenu(FToolMenuSection& InSection)
{
	UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
	if (!Context)
	{
		return;			
	}
	
	TArray<UObject*> SelectedObjects = Context->GetSelectedObjects();
	if (SelectedObjects.IsEmpty())
	{
		return;
	}

	// change menu label if anim blueprint is selected
	static const FText AnimAssetLabel(LOCTEXT("RetargetAnimationAsset", "Duplicate and Retarget Animation Assets"));
	static const FText AnimBlueprintLabel(LOCTEXT("RetargetAnimationBlueprint", "Duplicate and Retarget Animation Blueprint"));

	const FText MenuLabel = Cast<UAnimBlueprint>(SelectedObjects[0]) == nullptr ? AnimAssetLabel : AnimBlueprintLabel;
	
	InSection.AddMenuEntry(
		"IKRetargetToDifferentSkeleton",
		MenuLabel,
		LOCTEXT("RetargetAnimation_ToolTip", "Duplicate an animation asset and retarget to a different skeleton."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCurveEditor.TabIcon"),
		FUIAction(FExecuteAction::CreateLambda([SelectedObjects]()
		{
			SRetargetAnimAssetsWindow::ShowWindow(SelectedObjects);
		}),
		FCanExecuteAction::CreateLambda([InSection]()
		{
			if (UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>())
			{
				TArray<UObject*> SelectedObjects = Context->GetSelectedObjects();
				for (UObject* SelectedObject : SelectedObjects)
				{
					if (Cast<UAnimationAsset>(SelectedObject) || Cast<UAnimBlueprint>(SelectedObject))
					{
						return true;
					}
				}		
			}
			
			return false;
		}))
	);
}

const TArray<FText>& FAssetTypeActions_IKRetargeter::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AnimIKRigSubMenu", "IK Rig")
	};
	return SubMenus;
}

#undef LOCTEXT_NAMESPACE
