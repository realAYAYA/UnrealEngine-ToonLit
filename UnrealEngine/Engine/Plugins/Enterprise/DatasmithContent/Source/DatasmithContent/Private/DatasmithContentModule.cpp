// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithContentModule.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogDatasmithContent)

/**
 * DatasmithContent module implementation (private)
 */
class FDatasmithContentModule : public IDatasmithContentModule
{
public:
	virtual void StartupModule() override
	{
		// Create temporary directory which will be used by UDatasmithStaticMeshCADImportData to store transient data
		TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DatasmithContentTemp"));
		IFileManager::Get().MakeDirectory(*TempDir);
	}

	virtual void ShutdownModule() override
	{
		// Clean up all transient files created during the process
		IFileManager::Get().DeleteDirectory(*TempDir, false, true);
	}

	const FString& GetTempDir() const override
	{
		return TempDir;
	}

private:
	FString TempDir;
};

IMPLEMENT_MODULE(FDatasmithContentModule, DatasmithContent);
