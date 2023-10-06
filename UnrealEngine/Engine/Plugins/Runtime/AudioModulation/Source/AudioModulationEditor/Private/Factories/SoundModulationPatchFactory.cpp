// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundModulationPatchFactory.h"

#include "AudioAnalytics.h"
#include "SoundModulationPatch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundModulationPatchFactory)


USoundModulationPatchFactory::USoundModulationPatchFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundModulationPatch::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundModulationPatchFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("AudioModulation.ParameterPatchCreated"));
	return NewObject<USoundModulationPatch>(InParent, Name, Flags);
}

