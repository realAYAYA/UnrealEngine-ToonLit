// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorOnlyModifierFactory.h"

#include "EditorOnlyVCamModifier.h"
#include "Modifier/VCamModifier.h"

#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "UEditorOnlyModifierFactory"

UEditorOnlyModifierFactory::UEditorOnlyModifierFactory()
{
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UEditorOnlyVCamModifier::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

FText UEditorOnlyModifierFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "VCam Editor Only Modifier");
}

FText UEditorOnlyModifierFactory::GetToolTip() const
{
	return LOCTEXT("Tooltip", "Editor only modifiers only exist in the editor. They are safely excluded from VCam components in cooked builds.");
}

UObject* UEditorOnlyModifierFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UBlueprint* ModifierBlueprint = nullptr;
	if (ensure(SupportedClass == Class))
	{
		ensure(0 != (RF_Public & Flags));
		ModifierBlueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		if (TSubclassOf<UObject> GeneratedClass = ModifierBlueprint->GeneratedClass)
		{
			if (UEditorOnlyVCamModifier* DefaultSubject = GeneratedClass->GetDefaultObject<UEditorOnlyVCamModifier>())
			{
				DefaultSubject->InputMappingContext = InputMappingContext;
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(ModifierBlueprint);
	}
	return ModifierBlueprint;	
}

uint32 UEditorOnlyModifierFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("VirtualCamera", LOCTEXT("AssetCategoryName", "VCam"));
}

#undef LOCTEXT_NAMESPACE
