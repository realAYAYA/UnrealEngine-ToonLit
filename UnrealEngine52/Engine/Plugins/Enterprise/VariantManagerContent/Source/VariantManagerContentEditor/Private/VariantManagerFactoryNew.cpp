// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariantManagerFactoryNew.h"

#include "LevelVariantSets.h"

UVariantManagerFactoryNew::UVariantManagerFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = ULevelVariantSets::StaticClass();
}

UObject* UVariantManagerFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ULevelVariantSets* VariantSets = NewObject<ULevelVariantSets>(InParent, Name, Flags|RF_Transactional);

	return VariantSets;
}

bool UVariantManagerFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
