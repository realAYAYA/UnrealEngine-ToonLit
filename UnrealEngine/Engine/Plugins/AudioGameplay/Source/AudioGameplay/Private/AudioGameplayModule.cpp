// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioGameplayModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAudioGameplay"

DEFINE_LOG_CATEGORY(AudioGameplayLog);

void FAudioGameplayModule::StartupModule()
{
}

void FAudioGameplayModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAudioGameplayModule, AudioGameplay)