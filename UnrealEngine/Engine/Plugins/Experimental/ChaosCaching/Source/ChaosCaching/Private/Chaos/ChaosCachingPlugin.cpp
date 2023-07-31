// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosCachingPlugin.h"
#include "Chaos/Adapters/GeometryCollectionComponentCacheAdapter.h"
#include "Chaos/Adapters/StaticMeshComponentCacheAdapter.h"
#include "Chaos/Sequencer/ChaosCacheObjectSpawner.h"
#include "ILevelSequenceModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(IChaosCachingPlugin, ChaosCaching)

DEFINE_LOG_CATEGORY(LogChaosCache)

void IChaosCachingPlugin::StartupModule()
{
	GeometryCollectionAdapter = MakeUnique<Chaos::FGeometryCollectionCacheAdapter>();
	StaticMeshAdapter = MakeUnique<Chaos::FStaticMeshCacheAdapter>();

	ILevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<ILevelSequenceModule>("LevelSequence");
	OnCreateMovieSceneObjectSpawnerHandle = LevelSequenceModule.RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FChaosCacheObjectSpawner::CreateObjectSpawner));
	
	RegisterAdapter(GeometryCollectionAdapter.Get());
	RegisterAdapter(StaticMeshAdapter.Get());
}

void IChaosCachingPlugin::ShutdownModule() 
{
	UnregisterAdapter(StaticMeshAdapter.Get());
	UnregisterAdapter(GeometryCollectionAdapter.Get());

	StaticMeshAdapter = nullptr;
	GeometryCollectionAdapter = nullptr;

	ILevelSequenceModule* LevelSequenceModule = FModuleManager::GetModulePtr<ILevelSequenceModule>("LevelSequence");
	if (LevelSequenceModule)
	{
		LevelSequenceModule->UnregisterObjectSpawner(OnCreateMovieSceneObjectSpawnerHandle);
	}
}
