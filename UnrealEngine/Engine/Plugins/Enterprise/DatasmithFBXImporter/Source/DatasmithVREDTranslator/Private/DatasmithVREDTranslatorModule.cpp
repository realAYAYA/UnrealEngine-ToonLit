// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithVREDTranslatorModule.h"

#include "DatasmithVREDImporterMaterialSelector.h"
#include "DatasmithVREDTranslator.h"
#include "ReferenceMaterials/DatasmithReferenceMaterialManager.h"

#include "CoreMinimal.h"
#include "DatasmithTranslator.h"

class FVREDTranslatorModule : public IDatasmithVREDTranslatorModule
{
public:
	virtual void StartupModule() override
	{
		// Make sure the DatasmithImporter module exists and has been initialized before adding FDatasmithVREDTranslator's material selector
		FModuleManager::Get().LoadModule(TEXT("DatasmithImporter"));

		FDatasmithReferenceMaterialManager::Get().RegisterSelector(TEXT("VRED"), MakeShared< FDatasmithVREDImporterMaterialSelector >());

		Datasmith::RegisterTranslator<FDatasmithVREDTranslator>();
	}

	virtual void ShutdownModule() override
	{
		Datasmith::UnregisterTranslator<FDatasmithVREDTranslator>();
	}
};

IMPLEMENT_MODULE(FVREDTranslatorModule, DatasmithVREDTranslator);
