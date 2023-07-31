// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamModifierFactory.h"
#include "VCamModifier.h"

#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "VCamModifierFactory"

UVCamModifierFactory::UVCamModifierFactory()
{
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UVCamBlueprintModifier::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

FText UVCamModifierFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "VCam Modifier");
}

FText UVCamModifierFactory::GetToolTip() const
{
	return LOCTEXT("Tooltip", "Modifiers can manipulate the camera associated with a VCam Component. You can create custom modifiers with Blueprints or C++ and add them to the modifier stack of a VCam Component to layer different effects.");
}

UObject* UVCamModifierFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UBlueprint* ModifierBlueprint = nullptr;
	if (ensure(SupportedClass == Class))
	{
		ensure(0 != (RF_Public & Flags));
		ModifierBlueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		if (TSubclassOf<UObject> GeneratedClass = ModifierBlueprint->GeneratedClass)
		{
			if (UVCamBlueprintModifier* DefaultSubject = GeneratedClass->GetDefaultObject<UVCamBlueprintModifier>())
			{
				DefaultSubject->InputMappingContext = InputMappingContext;
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(ModifierBlueprint);
	}
	return ModifierBlueprint;	
}

uint32 UVCamModifierFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("VirtualCamera", LOCTEXT("AssetCategoryName", "VCam"));
}

#undef LOCTEXT_NAMESPACE
