// Copyright Epic Games, Inc. All Rights Reserved.

#include "AlembicHairTranslatorModule.h"

#include "AlembicHairTranslator.h"
#include "HairStrandsEditor.h"
#include "HAL/LowLevelMemTracker.h"

LLM_DECLARE_TAG_API(GroomEditor, HAIRSTRANDSEDITOR_API);

IMPLEMENT_MODULE(FAlembicHairTranslatorModule, AlembicHairTranslatorModule);

void FAlembicHairTranslatorModule::StartupModule()
{
	LLM_SCOPE_BYTAG(GroomEditor)
	FGroomEditor::Get().RegisterHairTranslator<FAlembicHairTranslator>();
}

void FAlembicHairTranslatorModule::ShutdownModule()
{
}

