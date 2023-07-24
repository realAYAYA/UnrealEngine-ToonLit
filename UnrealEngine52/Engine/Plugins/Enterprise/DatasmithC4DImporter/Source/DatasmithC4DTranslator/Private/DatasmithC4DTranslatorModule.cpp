// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithC4DTranslatorModule.h"

#ifdef _MELANGE_SDK_

#include "DatasmithC4DTranslator.h"
#include "DatasmithC4DImporterMaterialSelector.h"

#include "ReferenceMaterials/DatasmithReferenceMaterialManager.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#include "Internationalization/Text.h"
#include "Misc/MessageDialog.h"

#include "Misc/App.h"
#include "Internationalization/Internationalization.h"

#include <vector>

#define LOCTEXT_NAMESPACE "DatasmithC4DTranslatorModule"


class FC4DTranslatorModule : public IDatasmithC4DTranslatorModule
{
public:

	FC4DTranslatorModule()
	{
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		// Make sure the DatasmithImporter module exists and has been initialized before adding FDatasmithC4DTranslator's material selector
		FModuleManager::Get().LoadModule(TEXT("DatasmithTranslator"));

		FDatasmithReferenceMaterialManager::Get().RegisterSelector(TEXT("C4DTranslator"), MakeShared< FDatasmithC4DImporterMaterialSelector >());

		FString EnvVariable = FPlatformMisc::GetEnvironmentVariable(TEXT("DATASMITHC4D_DEBUG"));
		bDebugMode = !EnvVariable.IsEmpty();

		// TODO: Load C4DDynamicImporter if available else Import Static

		Datasmith::RegisterTranslator<FDatasmithC4DTranslator>();
	}

	virtual void ShutdownModule() override
	{
		Datasmith::UnregisterTranslator<FDatasmithC4DTranslator>();
	}

	bool InDebugMode() const override
	{
		return bDebugMode;
	}

private:

	/** Indicates if DATASMITHC4D_DEBUG environment variable is set */
	bool bDebugMode;
};

#else //_MELANGE_SDK_

// If the _MELANGE_SDK_ is not part of the build, create an empty module
class FC4DTranslatorModule : public IDatasmithC4DTranslatorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	bool InDebugMode() const override
	{
		return false;
	}
};
#endif //_MELANGE_SDK_

IMPLEMENT_MODULE(FC4DTranslatorModule, DatasmithC4DTranslator);

#undef LOCTEXT_NAMESPACE
