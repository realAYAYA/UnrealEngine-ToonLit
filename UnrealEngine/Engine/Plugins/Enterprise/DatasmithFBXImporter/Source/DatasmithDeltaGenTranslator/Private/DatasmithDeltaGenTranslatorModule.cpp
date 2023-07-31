// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithDeltaGenTranslatorModule.h"
#include "DatasmithDeltaGenTranslator.h"
#include "ReferenceMaterials/DatasmithReferenceMaterialManager.h"
#include "DatasmithDeltaGenImporterMaterialSelector.h"

#include "CoreMinimal.h"
#include "DatasmithTranslator.h"

class FDeltaGenTranslatorModule : public IDatasmithDeltaGenTranslatorModule
{
public:
	virtual void StartupModule() override
	{
		// Make sure the DatasmithImporter module exists and has been initialized before adding FDatasmithDeltaGenTranslator's material selector
		FModuleManager::Get().LoadModule(TEXT("DatasmithTranslator"));

		FDatasmithReferenceMaterialManager::Get().RegisterSelector(TEXT("Deltagen"), MakeShared< FDatasmithDeltaGenImporterMaterialSelector >());

		Datasmith::RegisterTranslator<FDatasmithDeltaGenTranslator>();
	}

	virtual void ShutdownModule() override
	{
		Datasmith::UnregisterTranslator<FDatasmithDeltaGenTranslator>();
	}
};

IMPLEMENT_MODULE(FDeltaGenTranslatorModule, DatasmithDeltaGenTranslator);
