// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDspModule.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

#include "HarmonixDsp/GainMatrix.h"
#include "HarmonixDsp/GainTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogHarmonixDspModule, Log, Log)

void FHarmonixDspModule::StartupModule()
{
	FGainMatrix::Init();
	FGainTable::Init();
}

void FHarmonixDspModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FHarmonixDspModule, HarmonixDsp);
