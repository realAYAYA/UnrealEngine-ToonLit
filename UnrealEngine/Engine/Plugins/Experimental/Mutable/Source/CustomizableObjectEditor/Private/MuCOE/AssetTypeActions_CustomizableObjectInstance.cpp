// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/AssetTypeActions_CustomizableObjectInstance.h"

#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformCrt.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Textures/SlateIcon.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Object.h"

class IToolkitHost;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_CustomizableObjectInstance::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	TArray<TWeakObjectPtr<UCustomizableObjectInstance>> Objects = GetTypedWeakObjectPtrs<UCustomizableObjectInstance>(InObjects);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObjectInstance", "CustomizableObjectInstance_Edit", "Edit"),
		NSLOCTEXT("AssetTypeActions_CustomizableObjectInstance", "CustomizableObjectInstance_EditTooltip", "Opens the selected instances in the static mesh editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CustomizableObjectInstance::ExecuteEdit, Objects ),
			FCanExecuteAction()
			)
		);

}


void FAssetTypeActions_CustomizableObjectInstance::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObjectInstance* Object = Cast<UCustomizableObjectInstance>(*ObjIt);
		if (Object != NULL)
		{
			ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>( "CustomizableObjectEditor" );
			CustomizableObjectEditorModule->CreateCustomizableObjectInstanceEditor(Mode, EditWithinLevelEditor, Object);
		}
	}
}


uint32 FAssetTypeActions_CustomizableObjectInstance::GetCategories()
{
	const ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	return CustomizableObjectEditorModule->GetAssetCategory();
}


void FAssetTypeActions_CustomizableObjectInstance::ExecuteEdit(TArray<TWeakObjectPtr<UCustomizableObjectInstance>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObjectInstance* Object = (*ObjIt).Get();
		if ( Object )
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
		}
	}
}


bool FAssetTypeActions_CustomizableObjectInstance::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened)
	{
		if (InObjects.Num() == 1)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InObjects[0]);
			return true;
		}
		else if (InObjects.Num() > 1)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(InObjects);
			return true;
		}
	}
	return false;
}


#undef LOCTEXT_NAMESPACE
