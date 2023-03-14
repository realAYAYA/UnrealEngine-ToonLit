// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynthesisModule.h"

#include "CoreMinimal.h"
#include "HAL/LowLevelMemTracker.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "UI/SynthSlateStyle.h"

DEFINE_LOG_CATEGORY(LogSynthesis);

IMPLEMENT_MODULE(FSynthesisModule, Synthesis)

void FSynthesisModule::StartupModule()
{
	LLM_SCOPE(ELLMTag::AudioSynthesis);
	FSynthSlateStyleSet::Initialize();

	FModuleManager::Get().LoadModuleChecked(TEXT("SignalProcessing"));
}

void FSynthesisModule::ShutdownModule()
{
	LLM_SCOPE(ELLMTag::AudioSynthesis);
	FSynthSlateStyleSet::Shutdown();
}


