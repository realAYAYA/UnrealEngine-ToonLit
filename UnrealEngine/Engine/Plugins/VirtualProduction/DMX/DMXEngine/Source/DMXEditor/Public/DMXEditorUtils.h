// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Library/DMXEntity.h"

class UDMXLibrary;
class UDMXEntityFixtureType;
class UDMXEntityFader;
class UDMXEntityFixturePatch;

/** 
 * Generic Editor Utilities.
 * for Fixture Type, refer to DMXFixtureTypeSharedData instead.
 */
class DMXEDITOR_API FDMXEditorUtils
{
public:
	typedef TArray<UDMXEntityFixturePatch*> FUnassignedPatchesArray;

	UE_DEPRECATED(5.0, "Moved to DMXRuntimeUtils to allow using it at runtime.")
	static FString GenerateUniqueNameFromExisting(const TSet<FString>& InExistingNames, const FString& InBaseName);

	UE_DEPRECATED(5.0, "Deprecated in favor of UDMXRuntimeUtils::FindUniqueEntityName.")
	static FString FindUniqueEntityName(const UDMXLibrary* InLibrary, TSubclassOf<UDMXEntity> InEntityClass, const FString& InBaseName = TEXT(""));

	UE_DEPRECATED(5.0, "Deprecated to reduce redundant code. Instead use UDMXRuntimeUtils::FindUniqueEntityName.")
	static void SetNewFixtureFunctionsNames(UDMXEntityFixtureType* InFixtureType);

	UE_DEPRECATED(5.0, "Deprecated in favor of new UDMXEntityFixtureType::CreateFixtureType and UDMXEntityFixturePatch::CreateFixturePatch that support creating patches in blueprints.")
	static bool AddEntity(UDMXLibrary* InLibrary, const FString& NewEntityName, TSubclassOf<UDMXEntity> NewEntityClass, UDMXEntity** OutNewEntity = nullptr);

	/**
	 * Validates an Entity name, also checking for uniqueness among others of the same type.
	 * @param NewEntityName		The name to validate.
	 * @param InLibrary			The DMXLibrary object to check for name uniqueness.
	 * @param InEntityClass		The type to check other Entities' names
	 * @param OutReason			If false is returned, contains a text with the reason for it.
	 * @return True if the name would be a valid one.
	 */
	static bool ValidateEntityName(const FString& NewEntityName, const UDMXLibrary* InLibrary, UClass* InEntityClass, FText& OutReason);

	/**  Renames an Entity */
	static void RenameEntity(UDMXLibrary* InLibrary, UDMXEntity* InEntity, const FString& NewName);

	/**  Checks if the Entity is being referenced by other objects. */
	static bool IsEntityUsed(const UDMXLibrary* InLibrary, const UDMXEntity* InEntity);

	/** DEPRECATED 5.0 */
	UE_DEPRECATED(5.0, "Replaced with UDMXEntityFixtureType::RemoveFixtureTypeFromLibrary and UDMXEntityFixturePatch::RemoveFixturePatchFromLibrary to support blueprint and runtime changes.")
	static void RemoveEntities(UDMXLibrary* InLibrary, const TArray<UDMXEntity*>& InEntities);

	/**  Copies Entities to the operating system's clipboard. */
	static void CopyEntities(const TArray<UDMXEntity*>&& EntitiesToCopy);

	/**  Determines whether the current contents of the clipboard contain paste-able DMX Entity information */
	static bool CanPasteEntities(UDMXLibrary* ParentLibrary);

	/** DEPRECATED 5.0 */
	UE_DEPRECATED(5.0, "Replaced with FDMXEditorUtils::CreateEntitiesFromClipboard. NOTE: GetEntitiesFromClipboard no longer returns any entities, as entities can only be created within a DMXLibrary.")
	static void GetEntitiesFromClipboard(TArray<UDMXEntity*>& OutNewObjects);

	/**
	 * Creates the copied DMX Entities from the clipboard without attempting to paste/apply them in any way
	 * 
	 * @param ParentLibrary			The library in which the entities are created.
	 * @return						The array of newly created enities.
	 */
	static TArray<UDMXEntity*> CreateEntitiesFromClipboard(UDMXLibrary* ParentLibrary);

	/**
	 * Compares the property values of two Fixture Types, including properties in arrays,
	 * and returns true if they are almost all the same.
	 * Name, ID and Parent Library are ignored.
	 */
	static bool AreFixtureTypesIdentical(const UDMXEntityFixtureType* A, const UDMXEntityFixtureType* B);

	/**  Returns the Entity class type name (e.g: Fixture Type for UDMXEntityFixtureType) in singular or plural */
	static FText GetEntityTypeNameText(TSubclassOf<UDMXEntity> EntityClass, bool bPlural = false);

	/** DEPRECATED 5.1 */
	UE_DEPRECATED(5.1, "Simplyifing AutoAssign, just use FDMXEditorUtils::AutoAssignedAddresses instead.")
	static bool TryAutoAssignToUniverses(UDMXEntityFixturePatch* Patch, const TSet<int32>& AllowedUniverses);

	/** DEPRECATED 5.1 */
	UE_DEPRECATED(5.1, "The bAutoAssign property got deprecated in UDMXEntityFixturePatch, so there's no meaning to ever auto assign on Fixture Type changes. So this function is removed without replacement.")
	static void AutoAssignedAddresses(UDMXEntityFixtureType* ChangedParentFixtureType);

	/** DEPRECATED 5.1 */
	UE_DEPRECATED(5.1, "Simplyifing AutoAssign, just use FDMXEditorUtils::AutoAssignedChannels instead.")
	static FUnassignedPatchesArray AutoAssignedAddresses(const TArray<UDMXEntityFixturePatch*>& ChangedFixturePatches, int32 MinimumAddress = 1, bool bCanChangePatchUniverses = true);

	/**
	 * Updates starting addresses for fixture patches that have bAutoAssignAddress set, ignores others.
	 * Note, patches all have to reside in the same library.
	 *
	 * The caller is responsible to call Modify on the patches and register undo/redo.
	 *
	 * If bCanChangePatchUniverses = true, this function will assign patches that do not fit into the existing universes to
	 * the next universe. MinimumAddress is ignored for the new universe, i.e. we start placing remaining patches at position 1.
	 *
	 * @param bAllowDecrementUniverse	If true, the patches can be assigend to a previous universe
	 * @param bCanChangePatchUniverses	If true, the patches can lower their absoulte channel
	 * @param FixturePatches			The patches that want their channels to be auto assigned
	 */
	static void AutoAssignedChannels(bool bAllowDecrementUniverse, bool bAllowDecrementChannels, TArray<UDMXEntityFixturePatch*> FixturePatches);
	
	/**
	 * Creates a unique color for all patches that use the default color FLinearColor(1.0f, 0.0f, 1.0f)
	 *
	 * @param Library				The library the patches resides in.
	 */
	static void UpdatePatchColors(UDMXLibrary* Library);

	/**
	 * Retrieve all assets for a given class via the asset registry. Will load into memory if needed.
	 *
	 * @param Class					The class to lookup.
	 * @param OutObjects			All found objects.
	 * 
	 */
	static void GetAllAssetsOfClass(UClass* Class, TArray<UObject*>& OutObjects);

	/**
	 * Locate universe conflicts between libraries
	 *
	 * @param Library					The library to be tested.
	 * @param OutConflictMessage		Message containing found conflicts
	 * @return							True if there is at least one conflict found.
	 */
	static bool DoesLibraryHaveUniverseConflicts(UDMXLibrary* Library, FText& OutInputPortConflictMessage, FText& OutOutputPortConflictMessage);

	/** Zeros memory in all active DMX buffers of all protocols */
	static void ClearAllDMXPortBuffers();

	/** Clears cached data fixture patches received */
	static void ClearFixturePatchCachedData();

	/** Gets the package or creates a new one if it doesn't exist */
	static UPackage* GetOrCreatePackage(TWeakObjectPtr<UObject> Parent, const FString& DesiredName);

	// can't instantiate this class
	FDMXEditorUtils() = delete;
};
