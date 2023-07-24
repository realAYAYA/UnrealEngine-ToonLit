// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFTranslatorModule.h"
#include "DatasmithGLTFImportOptions.h" // IWYU pragma: keep
#include "DatasmithGLTFTranslator.h"


const TCHAR* IDatasmithGLTFTranslatorModule::ModuleName = TEXT("DatasmithGLTFTranslator");

class FGLTFTranslatorModule : public IDatasmithGLTFTranslatorModule
{
public:
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModule(TEXT("DatasmithTranslator"));
		Datasmith::RegisterTranslator<FDatasmithGLTFTranslator>();
	}

	virtual void ShutdownModule() override
	{
		Datasmith::UnregisterTranslator<FDatasmithGLTFTranslator>();
	}
};

IMPLEMENT_MODULE(FGLTFTranslatorModule, DatasmithGLTFTranslator);
