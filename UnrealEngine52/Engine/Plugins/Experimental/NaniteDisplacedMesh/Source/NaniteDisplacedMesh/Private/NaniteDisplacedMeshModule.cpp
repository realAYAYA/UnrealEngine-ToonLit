// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "NaniteDisplacedMeshCompiler.h"

#define LOCTEXT_NAMESPACE "NaniteDisplacedMesh"

class FNaniteDisplacedMeshModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		FAssetCompilingManager::Get().RegisterManager(&FNaniteDisplacedMeshCompilingManager::Get());
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		FAssetCompilingManager::Get().UnregisterManager(&FNaniteDisplacedMeshCompilingManager::Get());
#endif
	}
};

IMPLEMENT_MODULE(FNaniteDisplacedMeshModule, NaniteDisplacedMesh);

#undef LOCTEXT_NAMESPACE
