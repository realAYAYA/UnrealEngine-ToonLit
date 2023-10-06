// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraContextEffectsLibraryFactory.h"

#include "Feedback/ContextEffects/LyraContextEffectsLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraContextEffectsLibraryFactory)

class FFeedbackContext;
class UClass;
class UObject;

ULyraContextEffectsLibraryFactory::ULyraContextEffectsLibraryFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = ULyraContextEffectsLibrary::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* ULyraContextEffectsLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ULyraContextEffectsLibrary* LyraContextEffectsLibrary = NewObject<ULyraContextEffectsLibrary>(InParent, Name, Flags);

	return LyraContextEffectsLibrary;
}
