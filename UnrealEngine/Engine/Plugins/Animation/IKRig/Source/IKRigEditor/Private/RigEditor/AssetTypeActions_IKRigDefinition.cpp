// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/AssetTypeActions_IKRigDefinition.h"
#include "IKRigDefinition.h"
#include "RigEditor/IKRigToolkit.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


UClass* FAssetTypeActions_IKRigDefinition::GetSupportedClass() const
{
	return UIKRigDefinition::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_IKRigDefinition::GetThumbnailInfo(UObject* Asset) const
{
	UIKRigDefinition* IKRig = CastChecked<UIKRigDefinition>(Asset);
	return NewObject<USceneThumbnailInfo>(IKRig, NAME_None, RF_Transactional);
}

void FAssetTypeActions_IKRigDefinition::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
    
    for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
    {
    	if (UIKRigDefinition* Asset = Cast<UIKRigDefinition>(*ObjIt))
    	{
    		TSharedRef<FIKRigEditorToolkit> NewEditor(new FIKRigEditorToolkit());
    		NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, Asset);
    	}
    }
}

const TArray<FText>& FAssetTypeActions_IKRigDefinition::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AnimIKRigSubMenu", "IK Rig")
	};
	return SubMenus;
}

#undef LOCTEXT_NAMESPACE
