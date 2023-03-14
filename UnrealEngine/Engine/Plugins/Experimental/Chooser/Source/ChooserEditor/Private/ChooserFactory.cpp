// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserFactory.h"
#include "Chooser.h"

UChooserTableFactory::UChooserTableFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UChooserTable::StaticClass();
}

bool UChooserTableFactory::ConfigureProperties()
{
	return true;
}

UObject* UChooserTableFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return NewObject<UChooserTable>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);
}