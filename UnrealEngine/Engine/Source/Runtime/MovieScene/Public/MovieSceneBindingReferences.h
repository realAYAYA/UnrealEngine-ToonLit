// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "MovieSceneBindingReferences.generated.h"

class UWorld;
struct FWorldPartitionResolveData;


/**
 * An array of binding references
 */
USTRUCT()
struct FMovieSceneBindingReference
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ID;

	UPROPERTY()
	FUniversalObjectLocator Locator;

	UPROPERTY()
	ELocatorResolveFlags ResolveFlags = ELocatorResolveFlags::None;

	void InitializeLocatorResolveFlags();
};


/**
 * Structure that stores a one to many mapping from object binding ID, to object references that pertain to that ID.
 */
USTRUCT()
struct FMovieSceneBindingReferences
{
	GENERATED_BODY()

	MOVIESCENE_API TArrayView<const FMovieSceneBindingReference> GetAllReferences() const;

	MOVIESCENE_API TArrayView<const FMovieSceneBindingReference> GetReferences(const FGuid& ObjectId) const;

	/**
	 * Check whether this map has a binding for the specified object id
	 * @return true if this map contains a binding for the id, false otherwise
	 */
	MOVIESCENE_API bool HasBinding(const FGuid& ObjectId) const;

	/**
	 * Remove a binding for the specified ID
	 *
	 * @param ObjectId	The ID to remove
	 */
	MOVIESCENE_API void RemoveBinding(const FGuid& ObjectId);

	/**
	 * Remove specific object references
	 *
	 * @param ObjectId	The ID to remove
	 * @param InObjects The objects to remove
	 * @param InContext A context in which InObject resides (either a UWorld, or an AActor)
	 */
	MOVIESCENE_API void RemoveObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject *InContext);

	/**
	 * Remove specific object references that do not resolve
	 *
	 * @param ObjectId	The ID to remove
	 * @param InContext A context in which InObject resides (either a UWorld, or an AActor)
	 */
	MOVIESCENE_API void RemoveInvalidObjects(const FGuid& ObjectId, UObject *InContext);

	/**
	 * Add a binding for the specified ID
	 *
	 * @param ObjectId	The ID to associate the object with
	 * @param InContext	A context in which InObject resides (either a UWorld, or an AActor)
	 */
	MOVIESCENE_API const FMovieSceneBindingReference* AddBinding(const FGuid& ObjectId, FUniversalObjectLocator&& NewLocator);

	/**
	 * Add a binding for the specified ID
	 *
	 * @param ObjectId	The ID to associate the object with
	 * @param InContext	A context in which InObject resides (either a UWorld, or an AActor)
	 */
	MOVIESCENE_API const FMovieSceneBindingReference* AddBinding(const FGuid& ObjectId, FUniversalObjectLocator&& NewLocator, ELocatorResolveFlags InResolveFlags);

	/**
	 * Resolve a binding for the specified ID using a given context
	 *
	 * @param ObjectId					The ID to associate the object with
	 * @param Params					Resolve parameters specifying the context and fragment-specific parameters
	 * @param OutObjects				Array to populate with resolved object bindings
	 */
	MOVIESCENE_API void ResolveBinding(const FGuid& ObjectId, const UE::UniversalObjectLocator::FResolveParams& ResolveParams, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const;

	/**
	 * Resolve a binding for the specified ID using a given context
	 *
	 * @param ObjectId					The ID to associate the object with
	 * @param InContext					A context in which InObject resides
	 * @oaram StreamedLevelAssetPath    The path to the streamed level asset that contains the level sequence actor playing back the sequence. 'None' for any non-instance-level setups.
	 * @param OutObjects				Array to populate with resolved object bindings
	 */
	MOVIESCENE_API FGuid FindBindingFromObject(UObject* InObject, UObject* InContext) const;

	/**
	 * Filter out any bindings that do not match the specified set of GUIDs
	 *
	 * @param ValidBindingIDs A set of GUIDs that are considered valid. Anything references not matching these will be removed.
	 */
	MOVIESCENE_API void RemoveInvalidBindings(const TSet<FGuid>& ValidBindingIDs);
	
	/**
	 * Unloads an object that has been loaded via a locator.
	 *  @param ObjectId	The ID of the binding to unload
	 *  @param BindingIndex	The index of the binding to unload
	 */
	MOVIESCENE_API void UnloadBoundObject(const UE::UniversalObjectLocator::FResolveParams& ResolveParams, const FGuid& ObjectId, int32 BindingIndex);

private:

	/** The map from object binding ID to an array of references that pertain to that ID */
	UPROPERTY()
	TArray<FMovieSceneBindingReference> SortedReferences;
};
