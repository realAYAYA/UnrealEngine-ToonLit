// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceFactory.h"
#include "AnimNextWorkspace.h"
#include "UObject/Package.h"

UAnimNextWorkspaceFactory::UAnimNextWorkspaceFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextWorkspace::StaticClass();
}

bool UAnimNextWorkspaceFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextWorkspaceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextWorkspace* NewParameter = NewObject<UAnimNextWorkspace>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	// make sure the package is never cooked.
	UPackage* Package = NewParameter->GetOutermost();
	Package->SetPackageFlags(Package->GetPackageFlags() | PKG_EditorOnly);

	return NewParameter;
}