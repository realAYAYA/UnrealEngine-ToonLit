// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioGameplayVolumeModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAudioGameplayVolume"

DEFINE_LOG_CATEGORY(AudioGameplayVolumeLog);

void FAudioGameplayVolumeModule::StartupModule()
{
}

void FAudioGameplayVolumeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAudioGameplayVolumeModule, AudioGameplayVolume)