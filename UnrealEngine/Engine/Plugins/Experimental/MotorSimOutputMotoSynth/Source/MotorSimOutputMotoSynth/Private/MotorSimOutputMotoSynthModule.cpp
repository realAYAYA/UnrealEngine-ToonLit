// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotorSimOutputMotoSynthModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FMotorSimOutputMotoSynthModule, MotorSimOutputMotoSynth)

void FMotorSimOutputMotoSynthModule::StartupModule()
{
	FModuleManager::Get().LoadModuleChecked(TEXT("SignalProcessing"));
}

void FMotorSimOutputMotoSynthModule::ShutdownModule()
{
}


