// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraRuntimeInstantiator.h"

#include "Core/CameraDirector.h"
#include "Core/CameraNode.h"
#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "IGameplayCamerasLiveEditManager.h"
#include "IGameplayCamerasModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

bool GGameplayCamerasEnableLiveEdit = true;
static FAutoConsoleVariableRef CVarGameplayCamerasEnableLiveEdit(
	TEXT("GameplayCameras.EnableLiveEdit"),
	GGameplayCamerasEnableLiveEdit,
	TEXT("(Default: true. Toggles live-editing of camera runtime objects.")
	);

bool GGameplayCamerasEnableInstantiationRecycling = true;
static FAutoConsoleVariableRef CVarGameplayCamerasEnableInstantiationRecycling(
	TEXT("GameplayCameras.EnableInstantiationRecycling"),
	GGameplayCamerasEnableInstantiationRecycling,
	TEXT("(Default: true. Toggles recycling of instantiated camera objects.")
	);

IGameplayCamerasModule* FCameraRuntimeInstantiator::GameplayCamerasModule(nullptr);

IGameplayCamerasModule& FCameraRuntimeInstantiator::GetGameplayCamerasModule()
{
	if (!GameplayCamerasModule)
	{
		GameplayCamerasModule = &FModuleManager::LoadModuleChecked<IGameplayCamerasModule>("GameplayCameras");
	}
	return *GameplayCamerasModule;
}

void FCameraRuntimeInstantiator::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : InstantiationPool)
	{
		FObjectPool& ObjectPool = Pair.Value;
		Collector.AddReferencedObjects(ObjectPool);
	}
}

UCameraNode* FCameraRuntimeInstantiator::InstantiateCameraNodeTree(const UCameraNode* InRootNode, const FCameraRuntimeInstantiationParams& Params)
{
	UCameraNode* NewRootNode = CastChecked<UCameraNode>(InstantiateObject(InRootNode, Params));
	return NewRootNode;
}

UCameraDirector* FCameraRuntimeInstantiator::InstantiateCameraDirector(const UCameraDirector* InCameraDirector, const FCameraRuntimeInstantiationParams& Params)
{
	UCameraDirector* NewCameraDirector = CastChecked<UCameraDirector>(InstantiateObject(InCameraDirector, Params));
	return NewCameraDirector;
}

UObject* FCameraRuntimeInstantiator::InstantiateObject(const UObject* InSource, const FCameraRuntimeInstantiationParams& Params)
{
	// See if we can recycle a previously instantiated object hierarchy.
	if (Params.bAllowRecyling && GGameplayCamerasEnableInstantiationRecycling)
	{
		if (FObjectPool* ObjectPool = InstantiationPool.Find(InSource))
		{
			TObjectPtr<UObject> Recycled = ObjectPool->Last();
			ObjectPool->RemoveAt(ObjectPool->Num() - 1);
			if (ObjectPool->IsEmpty())
			{
				InstantiationPool.Remove(InSource);
			}

			// Just return the head of the hierarchy that we remember.
			// The live-edit manager should already have the mapping of all
			// source-to-instantiated objects from the time the clone was
			// created (see below).
			return Recycled;
		}
	}

	// No recycled instance found... clone the source object hierarchy
	FObjectDuplicationParameters DuplicationParams = InitStaticDuplicateObjectParams(
			InSource,
			Params.InstantiationOuter,
			Params.InstantiationName);
#if WITH_EDITOR
	TMap<UObject*, UObject*> CreatedObjects;
	DuplicationParams.CreatedObjects = &CreatedObjects;
#endif  // WITH_EDITOR

	UObject* OutInstantiatedObject = StaticDuplicateObjectEx(DuplicationParams);

#if WITH_EDITOR
	// Register all instantiated objects into the live-edit manager for live-eding from the
	// source objects to their instantiated counterparts.
	// We only do this once on actual cloning. We expect the live-edit manager to hold onto
	// these objects until they are GC'ed.
	if (!IsRunningCommandlet() && GGameplayCamerasEnableLiveEdit)
	{
		if (TSharedPtr<IGameplayCamerasLiveEditManager> LiveEditManager = GetGameplayCamerasModule().GetLiveEditManager())
		{
			LiveEditManager->RegisterInstantiatedObjects(CreatedObjects);
		}
	}
#endif

	return OutInstantiatedObject;
}

void FCameraRuntimeInstantiator::RecycleInstantiatedObject(const UObject* InSourceObject, UObject* InstantiatedObject)
{
	FObjectPool& ObjectPool = InstantiationPool.FindOrAdd(InSourceObject);
	if (ObjectPool.Num() < 4)
	{
		ObjectPool.Add(InstantiatedObject);
	}
}

void FCameraRuntimeInstantiator::ForwardPropertyChange(const UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_EDITOR
	if (GGameplayCamerasEnableLiveEdit)
	{
		if (TSharedPtr<IGameplayCamerasLiveEditManager> LiveEditManager = GetGameplayCamerasModule().GetLiveEditManager())
		{
			LiveEditManager->ForwardPropertyChange(Object, PropertyChangedEvent);
		}
	}
#endif
}

