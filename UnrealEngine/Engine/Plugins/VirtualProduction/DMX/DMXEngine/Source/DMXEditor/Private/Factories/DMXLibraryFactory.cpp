// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXLibraryFactory.h"

#include "DMXEditorLog.h"
#include "DMXEditorModule.h"
#include "Library/DMXLibrary.h"


UDMXLibraryFactory::UDMXLibraryFactory()
{
	SupportedClass = UDMXLibrary::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDMXLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UDMXLibrary* DMXLibrary = NewObject<UDMXLibrary>(InParent, Name, Flags);

	return DMXLibrary;
}

uint32 UDMXLibraryFactory::GetMenuCategories() const
{
	return (uint32)FDMXEditorModule::GetAssetCategory();
}
