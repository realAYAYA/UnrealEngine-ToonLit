// Copyright Epic Games, Inc. All Rights Reserved.

#include "Field/FieldSystemFactory.h"
#include "Field/FieldSystemCoreAlgo.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "FieldSystem"

/////////////////////////////////////////////////////
// FieldSystemFactory

UFieldSystemFactory::UFieldSystemFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UFieldSystem::StaticClass();
}

UFieldSystem* UFieldSystemFactory::StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UFieldSystem* System = static_cast<UFieldSystem*>(NewObject<UFieldSystem>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone));
	System->MarkPackageDirty();
	return System;
}

UObject* UFieldSystemFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{

	UFieldSystem* NewFieldSystem = StaticFactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
	NewFieldSystem->MarkPackageDirty();
	return NewFieldSystem;
}

#undef LOCTEXT_NAMESPACE



