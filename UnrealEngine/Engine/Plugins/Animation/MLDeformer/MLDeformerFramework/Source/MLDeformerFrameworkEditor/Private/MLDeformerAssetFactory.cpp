// Copyright Epic Games, Inc. All Rights Reserved.


#include "MLDeformerAssetFactory.h"
#include "MLDeformerAsset.h"
#include "AssetTypeCategories.h"

#define LOCTEXT_NAMESPACE "MLDeformerAssetFactory"

UMLDeformerFactory::UMLDeformerFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMLDeformerAsset::StaticClass();
}

UObject* UMLDeformerFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMLDeformerAsset>(InParent, Name, Flags | RF_Transactional);;
}

bool UMLDeformerFactory::ShouldShowInNewMenu() const
{
	return true;
}

bool UMLDeformerFactory::ConfigureProperties()
{
	return true;
}

FText UMLDeformerFactory::GetDisplayName() const
{
	return LOCTEXT("MLDeformerAsset_DisplayName", "ML Deformer");
}

uint32 UMLDeformerFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Animation;
}

FText UMLDeformerFactory::GetToolTip() const
{
	return LOCTEXT("MLDeformerAsset_Tooltip", "Machine learning based mesh deformer asset.");
}

FString UMLDeformerFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("MLD_NewDeformer"));
}

#undef LOCTEXT_NAMESPACE
