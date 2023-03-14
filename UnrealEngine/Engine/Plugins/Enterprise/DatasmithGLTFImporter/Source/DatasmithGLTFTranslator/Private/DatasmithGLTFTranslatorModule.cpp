// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFTranslatorModule.h"
#include "DatasmithGLTFTranslator.h"

#include "CoreMinimal.h"
#include "DatasmithTranslator.h"
#include "DatasmithTranslatorModule.h"

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
