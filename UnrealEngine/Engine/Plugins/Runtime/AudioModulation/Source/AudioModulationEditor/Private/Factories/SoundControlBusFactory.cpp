// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundControlBusFactory.h"

#include "AudioAnalytics.h"
#include "SoundControlBus.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundControlBusFactory)


USoundControlBusFactory::USoundControlBusFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundControlBus::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundControlBusFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("AudioModulation.ControlBusCreated"));
	return NewObject<USoundControlBus>(InParent, Name, Flags);
}
