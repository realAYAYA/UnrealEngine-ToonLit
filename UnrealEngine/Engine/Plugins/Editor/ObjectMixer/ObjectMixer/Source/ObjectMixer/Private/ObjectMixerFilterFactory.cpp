// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectMixerFilterFactory.h"

#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"

#include "AssetToolsModule.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "ObjectMixerFilterFactory"

UObjectMixerBlueprintFilterFactory::UObjectMixerBlueprintFilterFactory()
{
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UObjectMixerBlueprintObjectFilter::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

FText UObjectMixerBlueprintFilterFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Object Mixer Filter");
}

UObject* UObjectMixerBlueprintFilterFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UBlueprint* FilterBlueprint = nullptr;
	if (ensure(SupportedClass == Class))
	{
		ensure(0 != (RF_Public & Flags));
		FilterBlueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	}
	return FilterBlueprint;	
}

FString UObjectMixerBlueprintFilterFactory::GetDefaultNewAssetName() const
{
	return "NewObjectMixerFilter";
}

uint32 UObjectMixerBlueprintFilterFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

#undef LOCTEXT_NAMESPACE
