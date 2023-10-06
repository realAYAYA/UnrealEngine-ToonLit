// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterLibraryFactory.h"
#include "Param/AnimNextParameterLibrary.h"
#include "UObject/Package.h"

UAnimNextParameterLibraryFactory::UAnimNextParameterLibraryFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextParameterLibrary::StaticClass();
}

bool UAnimNextParameterLibraryFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextParameterLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextParameterLibrary* NewParameter = NewObject<UAnimNextParameterLibrary>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	// make sure the package is never cooked.
	UPackage* Package = NewParameter->GetOutermost();
	Package->SetPackageFlags(Package->GetPackageFlags() | PKG_EditorOnly);

	return NewParameter;
}