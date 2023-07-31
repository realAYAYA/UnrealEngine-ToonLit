// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Sequencer/ChaosCacheObjectSpawner.h"
#include "Chaos/CacheManagerActor.h"
#include "MovieSceneSpawnable.h"
#include "UObject/Package.h"
#include "Engine/World.h"

FChaosCacheObjectSpawner::FChaosCacheObjectSpawner() : FLevelSequenceActorSpawner()
{}

TSharedRef<IMovieSceneObjectSpawner> FChaosCacheObjectSpawner::CreateObjectSpawner()
{
	return MakeShareable(new FChaosCacheObjectSpawner);
}

UClass* FChaosCacheObjectSpawner::GetSupportedTemplateType() const
{
	return AChaosCacheManager::StaticClass();
}

UObject* FChaosCacheObjectSpawner::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	UObject* SpawnedObject = FLevelSequenceActorSpawner::SpawnObject(Spawnable, TemplateID, Player);
	
	if (AChaosCacheManager* ChaosCache = Cast<AChaosCacheManager>(SpawnedObject))
	{
		for(FObservedComponent& ObservedComponent : ChaosCache->GetObservedComponents())
		{
			FString FullPath = ObservedComponent.SoftComponentRef.OtherActor.ToString();

#if WITH_EDITORONLY_DATA
			if (const UPackage* ObjectPackage = SpawnedObject->GetPackage())
			{
				// If this is being set from PIE we need to remove the pie prefix 
				if (ObjectPackage->GetPIEInstanceID() == INDEX_NONE)
				{
					int32 PIEInstanceID = INDEX_NONE;
					FullPath = UWorld::RemovePIEPrefix(FullPath, &PIEInstanceID);
				}
			}
#endif
			ObservedComponent.SoftComponentRef.OtherActor = FSoftObjectPath(FullPath);
		}
	}

	return SpawnedObject;
}

void FChaosCacheObjectSpawner::DestroySpawnedObject(UObject& Object)
{
	FLevelSequenceActorSpawner::DestroySpawnedObject(Object);
}
