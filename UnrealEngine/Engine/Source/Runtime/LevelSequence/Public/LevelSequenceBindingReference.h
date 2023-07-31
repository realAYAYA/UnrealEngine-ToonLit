// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/Guid.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "LevelSequenceBindingReference.generated.h"

/**
 * An external reference to an level sequence object, resolvable through an arbitrary context.
 * 
 * Bindings consist of an optional package name, and the path to the object within that package.
 * Where package name is empty, the reference is a relative path from a specific outer (the context).
 * Currently, the package name should only ever be empty for component references, which must remain relative bindings to work correctly with spawnables and reinstanced actors.
 */
USTRUCT()
struct FLevelSequenceBindingReference
{
	GENERATED_BODY();

	/**
	 * Default construction only used for serialization
	 */
	FLevelSequenceBindingReference() {}

	/**
	 * Construct a new binding reference from an object, and a given context (expected to be either a UWorld, or an AActor)
	 */
	LEVELSEQUENCE_API FLevelSequenceBindingReference(UObject* InObject, UObject* InContext);

	/**
	 * Resolve this reference within the specified context
	 *
	 * @param InContext		The context to resolve the binding within. Either a UWorld, ULevel (when playing in an instanced level) or an AActor where this binding relates to an actor component
	 * @param StreamedLevelAssetPath    The path to the streamed level asset that contains the level sequence actor playing back the sequence. A null path for any non-instance-level setups.
	 * @return The object (usually an Actor or an ActorComponent).
	 */
	LEVELSEQUENCE_API UObject* Resolve(UObject* InContext, const FTopLevelAssetPath& StreamedLevelAssetPath) const;
	
	UE_DEPRECATED(5.1, "Asset path FNames are deprecated. Use FTopLevelAssetPath instead.")
	UObject* Resolve(UObject* InContext, FName StreamedLevelAssetPath) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Resolve(InContext, FTopLevelAssetPath(StreamedLevelAssetPath));
PRAGMA_ENABLE_DEPRECATION_WARNINGS		
	}

	/**
	 * Check whether this binding reference is equal to the specified object
	 */
	LEVELSEQUENCE_API bool operator==(const FLevelSequenceBindingReference& Other) const;

	/** Handles ExternalObjectPath fixup */
	void PostSerialize(const FArchive& Ar);

private:

	/** Replaced by ExternalObjectPath */
	UPROPERTY()
	FString PackageName_DEPRECATED;

	/** Path to a specific actor/component inside an external package */
	UPROPERTY()
	FSoftObjectPath ExternalObjectPath;

	/** Object path relative to a passed in context object, this is used if ExternalObjectPath is invalid */
	UPROPERTY()
	FString ObjectPath;
};


template<>
struct TStructOpsTypeTraits<FLevelSequenceBindingReference> : public TStructOpsTypeTraitsBase2<FLevelSequenceBindingReference>
{
	enum
	{
		WithPostSerialize = true,
	};
};

/**
 * An array of binding references
 */
USTRUCT()
struct FLevelSequenceBindingReferenceArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FLevelSequenceBindingReference> References;
};


/**
 * Structure that stores a one to many mapping from object binding ID, to object references that pertain to that ID.
 */
USTRUCT()
struct FLevelSequenceBindingReferences
{
	GENERATED_BODY()

	/**
	 * Check whether this map has a binding for the specified object id
	 * @return true if this map contains a binding for the id, false otherwise
	 */
	bool HasBinding(const FGuid& ObjectId) const;

	/**
	 * Remove a binding for the specified ID
	 *
	 * @param ObjectId	The ID to remove
	 */
	void RemoveBinding(const FGuid& ObjectId);

	/**
	 * Remove specific object references
	 *
	 * @param ObjectId	The ID to remove
	 * @param InObjects The objects to remove
	 * @param InContext A context in which InObject resides (either a UWorld, or an AActor)
	 */
	void RemoveObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject *InContext);

	/**
	 * Remove specific object references that do not resolve
	 *
	 * @param ObjectId	The ID to remove
	 * @param InContext A context in which InObject resides (either a UWorld, or an AActor)
	 */
	void RemoveInvalidObjects(const FGuid& ObjectId, UObject *InContext);

	/**
	 * Add a binding for the specified ID
	 *
	 * @param ObjectId	The ID to associate the object with
	 * @param InObject	The object to associate
	 * @param InContext	A context in which InObject resides (either a UWorld, or an AActor)
	 */
	void AddBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext);

	/**
	 * Resolve a binding for the specified ID using a given context
	 *
	 * @param ObjectId					The ID to associate the object with
	 * @param InContext					A context in which InObject resides
	 * @oaram StreamedLevelAssetPath    The path to the streamed level asset that contains the level sequence actor playing back the sequence. 'None' for any non-instance-level setups.
	 * @param OutObjects				Array to populate with resolved object bindings
	 */
	void ResolveBinding(const FGuid& ObjectId, UObject* InContext, const FTopLevelAssetPath& StreamedLevelAssetPath, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const;

	/**
	 * Resolve a binding for the specified ID using a given context
	 *
	 * @param ObjectId					The ID to associate the object with
	 * @param InContext					A context in which InObject resides
	 * @oaram StreamedLevelAssetPath    The path to the streamed level asset that contains the level sequence actor playing back the sequence. 'None' for any non-instance-level setups.
	 * @param OutObjects				Array to populate with resolved object bindings
	 */
	FGuid FindBindingFromObject(UObject* InObject, UObject* InContext) const;

	/**
	 * Const accessor for the currently bound anim instance IDs
	 */
	const TSet<FGuid>& GetBoundAnimInstances() const
	{
		return AnimSequenceInstances;
	}

	/**
	 * Const accessor for the currently bound post process instance IDs
	 */
	const TSet<FGuid>& GetBoundPostProcessInstances() const
	{
		return PostProcessInstances;
	}

	/**
	 * Filter out any bindings that do not match the specified set of GUIDs
	 *
	 * @param ValidBindingIDs A set of GUIDs that are considered valid. Anything references not matching these will be removed.
	 */
	void RemoveInvalidBindings(const TSet<FGuid>& ValidBindingIDs);

private:

	/** The map from object binding ID to an array of references that pertain to that ID */
	UPROPERTY()
	TMap<FGuid, FLevelSequenceBindingReferenceArray> BindingIdToReferences;

	/** A set of object binding IDs that relate to anim sequence instances (must be a child of USkeletalMeshComponent) */
	UPROPERTY()
	TSet<FGuid> AnimSequenceInstances;

	/** A set of object binding IDs that relate to post process instances (must be a child of USkeletalMeshComponent) */
	UPROPERTY()
	TSet<FGuid> PostProcessInstances;
};
