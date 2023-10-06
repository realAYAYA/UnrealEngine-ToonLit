// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithCloTranslatorModule.h"
#include "DatasmithCloTranslator.h"
#include "DatasmithTranslator.h"


#define LOCTEXT_NAMESPACE "FDatasmithCloTranslatorModule"

void FDatasmithCloTranslatorModule::StartupModule()
{
	Datasmith::RegisterTranslator<FDatasmithCloTranslator>();
}

void FDatasmithCloTranslatorModule::ShutdownModule()
{
	Datasmith::UnregisterTranslator<FDatasmithCloTranslator>();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDatasmithCloTranslatorModule, DatasmithCloTranslator)
