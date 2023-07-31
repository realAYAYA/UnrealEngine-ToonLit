// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFImporterModule.h"

#include "GLTFImporterContext.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * glTF Importer module implementation (private)
 */
class FGLTFImporterModule : public IGLTFImporterModule
{
	FGLTFImporterContext ImporterContext;

public:
	virtual FGLTFImporterContext& GetImporterContext() override
	{
		return ImporterContext;
	}

	virtual void StartupModule() override {}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGLTFImporterModule, GLTFImporter);
