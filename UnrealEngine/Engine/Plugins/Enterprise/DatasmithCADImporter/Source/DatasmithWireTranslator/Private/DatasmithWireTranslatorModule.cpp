// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithWireTranslatorModule.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "DatasmithWireTranslator.h"

// need this macro wrapper to expand the UE_DATASMITHWIRETRANSLATOR_MODULE_NAME macro and not create symbols with "UE_DATASMITHWIRETRANSLATOR_MODULE_NAME" in the token
#define IMPLEMENT_MODULE_WRAPPER(ModuleName) IMPLEMENT_MODULE(UE_DATASMITHWIRETRANSLATOR_NAMESPACE::FDatasmithWireTranslatorModule, ModuleName);
IMPLEMENT_MODULE_WRAPPER(UE_DATASMITHWIRETRANSLATOR_MODULE_NAME)

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

void FDatasmithWireTranslatorModule::StartupModule()
{
	// Create temporary directory which will be used by CoreTech to store tessellation data
	TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("WireImportTemp"));
	IFileManager::Get().MakeDirectory(*TempDir);

	Datasmith::RegisterTranslator<FDatasmithWireTranslator>();
}

void FDatasmithWireTranslatorModule::ShutdownModule()
{
	Datasmith::UnregisterTranslator<FDatasmithWireTranslator>();
}

FString FDatasmithWireTranslatorModule::GetTempDir() const
{
	return TempDir;
}

}