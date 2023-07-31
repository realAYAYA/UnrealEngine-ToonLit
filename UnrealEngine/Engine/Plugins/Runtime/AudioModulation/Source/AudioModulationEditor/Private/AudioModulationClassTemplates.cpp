// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioModulationClassTemplates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioModulationClassTemplates)

USoundModulationClassTemplate::USoundModulationClassTemplate(const FObjectInitializer& ObjectInitializer)
	: UPluginClassTemplate(ObjectInitializer)
{
	PluginName = TEXT("AudioModulation");
}

USoundModulationGeneratorClassTemplate::USoundModulationGeneratorClassTemplate(const FObjectInitializer& ObjectInitializer)
	: USoundModulationClassTemplate(ObjectInitializer)
{
	SetGeneratedBaseClass(USoundModulationGenerator::StaticClass());
}

