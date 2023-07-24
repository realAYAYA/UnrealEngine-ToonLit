// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFImporterModule.h"

#include "GLTFAnimation.h"
#include "GLTFImporterContext.h"

#include "GLTFMaterial.h"
#include "GLTFMesh.h"
#include "GLTFNode.h"
#include "GLTFTexture.h"

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
