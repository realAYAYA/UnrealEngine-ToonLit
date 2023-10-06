// Copyright Epic Games, Inc. All Rights Reserved.
#include "DatasmithNativeTranslatorModule.h"

#include "DatasmithNativeTranslator.h"
#include "DatasmithTranslator.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDatasmithNativeTranslatorModule, DatasmithNativeTranslator);

void FDatasmithNativeTranslatorModule::StartupModule()
{
	Datasmith::RegisterTranslator<FDatasmithNativeTranslator>();
}

void FDatasmithNativeTranslatorModule::ShutdownModule()
{
	Datasmith::UnregisterTranslator<FDatasmithNativeTranslator>();
}
