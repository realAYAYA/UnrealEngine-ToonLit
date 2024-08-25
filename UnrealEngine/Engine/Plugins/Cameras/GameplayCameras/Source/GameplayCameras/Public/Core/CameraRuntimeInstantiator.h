// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class IGameplayCamerasModule;
class UCameraDirector;
class UCameraNode;
class UObject;

/**
 * Parameter structure for instantiating an object.
 */
struct FCameraRuntimeInstantiationParams
{
	UObject* InstantiationOuter = nullptr;
	FName InstantiationName = NAME_None;
	bool bAllowRecyling = false;
};

/**
 * Utility class for instanting source objects into the game.
 */
class FCameraRuntimeInstantiator
{
public:

	/** 
	 * Instantiates a tree of camera nodes by cloning it, and keeping track of the relationship between
	 * the clones and their source assets.
	 */
	UCameraNode* InstantiateCameraNodeTree(const UCameraNode* InRootNode, const FCameraRuntimeInstantiationParams& Params);

	/** 
	 * Instantiates a camera directory by cloning it, and keeping track of the relationship between
	 * the clone and its source asset.
	 */
	UCameraDirector* InstantiateCameraDirector(const UCameraDirector* InCameraDirector, const FCameraRuntimeInstantiationParams& Params);

	/**
	 * Release the root of an instantiated object hierarchy, for potential recycling later.
	 */
	void RecycleInstantiatedObject(const UObject* InSourceObject, UObject* InstantiatedObject);

	/**
	 * Take a property change applied on a source object and try to replicate it on any related instantiated objects.
	 */
	static void ForwardPropertyChange(const UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent);

public:

	// Internal API.
	void AddReferencedObjects(FReferenceCollector& Collector);

private:

	static IGameplayCamerasModule& GetGameplayCamerasModule();

	UObject* InstantiateObject(const UObject* InSource, const FCameraRuntimeInstantiationParams& Params);

private:

	static IGameplayCamerasModule* GameplayCamerasModule;

	using FObjectPool = TArray<TObjectPtr<UObject>>; //, TInlineAllocator<4>>;
	TMap<TWeakObjectPtr<const UObject>, FObjectPool> InstantiationPool;
};

