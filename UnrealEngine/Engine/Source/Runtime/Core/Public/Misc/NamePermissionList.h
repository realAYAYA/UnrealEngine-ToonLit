// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FString;

/** List of owner names that requested a specific item filtered, allows unregistering specific set of changes by a given plugin or system */
typedef TArray<FName> FPermissionListOwners;

class FNamePermissionList : public TSharedFromThis<FNamePermissionList>
{
public:
	FNamePermissionList() {}
	virtual ~FNamePermissionList() {}

	/** Returns true if passes filter restrictions using exact match */
	CORE_API bool PassesFilter(const FName Item) const;

	/** 
	 * Add item to DenyList, this specific item will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddDenyListItem(const FName OwnerName, const FName Item);

	/**
	 * Add item to allowlist after which all items not in the allowlist will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddAllowListItem(const FName OwnerName, const FName Item);

	/**
	 * Removes a previously-added item from the DenyList.
	 * @return whether the filters changed.
	 */
	CORE_API bool RemoveDenyListItem(const FName OwnerName, const FName Item);

	/**
	 * Removes a previously-added item from the allowlist.
	 * @return whether the filters changed.
	 */
	CORE_API bool RemoveAllowListItem(const FName OwnerName, const FName Item);

	/**
	 * Set to filter out all items.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddDenyListAll(const FName OwnerName);
	
	/** True if has filters active */
	CORE_API bool HasFiltering() const;

	/** Gathers the names of all the owners in this DenyList. */
	CORE_API TArray<FName> GetOwnerNames() const;

	/** 
	* Removes all filtering changes associated with a specific owner name.
	 * @return whether the filters changed.
	 */
	CORE_API bool UnregisterOwner(const FName OwnerName);

	/**
	 * Removes all filtering changes associated with the specified list of owner names.
	 * @return whether the filters changed.
	 */
	CORE_API bool UnregisterOwners(const TArray<FName>& OwnerNames);

	/**
	 * Add the specified filters to this one.
	 * @return whether the filters changed.
	 */
	CORE_API bool Append(const FNamePermissionList& Other);

	/**
	* Unregisters specified owners then adds specified filters in one operation (to avoid multiple filters changed events).
	* @return whether the filters changed.
	*/
	CORE_API bool UnregisterOwnersAndAppend(const TArray<FName>& OwnerNamesToRemove, const FNamePermissionList& FiltersToAdd);

	/** Get raw DenyList */
	const TMap<FName, FPermissionListOwners>& GetDenyList() const { return DenyList; }
	
	/** Get raw allowlist */
	const TMap<FName, FPermissionListOwners>& GetAllowList() const { return AllowList; }

	/** Are all items set to be filtered out */
	bool IsDenyListAll() const { return DenyListAll.Num() > 0; }

	/** Triggered when filter changes */
	FSimpleMulticastDelegate& OnFilterChanged() { return OnFilterChangedDelegate; }

protected:

	/** List if items to filter out */
	TMap<FName, FPermissionListOwners> DenyList;

	/** List of items to allow, if not empty all items will be filtered out unless they are in the list */
	TMap<FName, FPermissionListOwners> AllowList;

	/** List of owner names that requested all items to be filtered out */
	FPermissionListOwners DenyListAll;

	/** Triggered when filter changes */
	FSimpleMulticastDelegate OnFilterChangedDelegate;

	/** Temporarily prevent delegate from being triggered */
	bool bSuppressOnFilterChanged = false;
};

enum class EPathPermissionListType
{
	Default,	// Default path permission list
	ClassPaths	// Class permission list
};

class FPathPermissionList : public TSharedFromThis<FPathPermissionList>
{
public:
	FPathPermissionList(EPathPermissionListType InType = EPathPermissionListType::Default) 
		: ListType(InType)
	{
	}
	virtual ~FPathPermissionList() {}
	
	/** Returns true if passes filter restrictions using exact match */
	CORE_API bool PassesFilter(const FStringView Item) const;

	/** Returns true if passes filter restrictions using exact match */
	CORE_API bool PassesFilter(const FName Item) const;

	/** Returns true if passes filter restrictions using exact match */
	CORE_API bool PassesFilter(const TCHAR* Item) const;

	/** Returns true if passes filter restrictions for path */
	CORE_API bool PassesStartsWithFilter(const FStringView Item, const bool bAllowParentPaths = false) const;

	/** Returns true if passes filter restrictions for path */
	CORE_API bool PassesStartsWithFilter(const FName Item, const bool bAllowParentPaths = false) const;

	/** Returns true if passes filter restrictions for path */
	CORE_API bool PassesStartsWithFilter(const TCHAR* Item, const bool bAllowParentPaths = false) const;

	/** 
	 * Add item to DenyList, this specific item will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddDenyListItem(const FName OwnerName, const FStringView Item);

	/**
	 * Add item to DenyList, this specific item will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddDenyListItem(const FName OwnerName, const FName Item);

	/**
	 * Add item to DenyList, this specific item will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddDenyListItem(const FName OwnerName, const TCHAR* Item);

	/**
	* Remove item from the DenyList
	* @return whether the filters changed
	*/
	CORE_API bool RemoveDenyListItem(const FName OwnerName, const FStringView Item);

	/**
	* Remove item from the DenyList
	* @return whether the filters changed
	*/
	CORE_API bool RemoveDenyListItem(const FName OwnerName, const FName Item);

	/**
	* Remove item from the DenyList
	* @return whether the filters changed
	*/
	CORE_API bool RemoveDenyListItem(const FName OwnerName, const TCHAR* Item);

	/**
	 * Add item to allowlist after which all items not in the allowlist will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddAllowListItem(const FName OwnerName, const FStringView Item);

	/**
	 * Add item to allowlist after which all items not in the allowlist will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddAllowListItem(const FName OwnerName, const FName Item);

	/**
	 * Add item to allowlist after which all items not in the allowlist will be filtered out.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddAllowListItem(const FName OwnerName, const TCHAR* Item);

	/**
	* Remove item from the AllowList
	* @return whether the filters changed
	*/
	CORE_API bool RemoveAllowListItem(const FName OwnerName, const FStringView Item);

	/**
	* Remove item from the AllowList
	* @return whether the filters changed
	*/
	CORE_API bool RemoveAllowListItem(const FName OwnerName, const FName Item);

	/**
	* Remove item from the AllowList
	* @return whether the filters changed
	*/
	CORE_API bool RemoveAllowListItem(const FName OwnerName, const TCHAR* Item);

	/**
	 * Set to filter out all items.
	 * @return whether the filters changed.
	 */
	CORE_API bool AddDenyListAll(const FName OwnerName);
	
	/** True if has filters active */
	CORE_API bool HasFiltering() const;

	/** Gathers the names of all the owners in this DenyList. */
	CORE_API TArray<FName> GetOwnerNames() const;

	/**
	 * Removes all filtering changes associated with a specific owner name.
	 * @return whether the filters changed.
	 */
	CORE_API bool UnregisterOwner(const FName OwnerName);

	/**
	 * Removes all filtering changes associated with the specified list of owner names.
	 * @return whether the filters changed.
	 */
	CORE_API bool UnregisterOwners(const TArray<FName>& OwnerNames);
	
	/**
	 * Add the specified filters to this one. Rules are not applied, direct append lists.
	 * @return whether the filters changed.
	 */
	CORE_API bool Append(const FPathPermissionList& Other);

	/**
	 * Combine two filters.
	 * Result will contain all DenyList paths combined.
	 * Result will contain AllowList paths that pass both filters.
	 * @return new combined filter.
	 */
	CORE_API FPathPermissionList CombinePathFilters(const FPathPermissionList& OtherFilter) const;

	/**
	* Unregisters specified owners then adds specified filters in one operation (to avoid multiple filters changed events).
	* @return whether the filters changed.
	*/
	CORE_API bool UnregisterOwnersAndAppend(const TArray<FName>& OwnerNamesToRemove, const FPathPermissionList& FiltersToAdd);

	/** Get raw DenyList */
	const TMap<FString, FPermissionListOwners>& GetDenyList() const { return DenyList; }
	
	/** Get raw allowlist */
	const TMap<FString, FPermissionListOwners>& GetAllowList() const { return AllowList; }

	/** Are all items set to be filtered out */
	bool IsDenyListAll() const { return DenyListAll.Num() > 0; }
	
	/** Triggered when filter changes */
	FSimpleMulticastDelegate& OnFilterChanged() { return OnFilterChangedDelegate; }

	/** Dumps the path permission list details into a multi-line string */
	CORE_API FString ToString() const;

protected:

	/**
	 * Checks if an item is of a valid format for this list
	 * @return True if the item passes list type test.
	 */
	CORE_API void VerifyItemMatchesListType(const FStringView Item) const;

	/** List if items to filter out */
	TMap<FString, FPermissionListOwners> DenyList;

	/** List of items to allow, if not empty all items will be filtered out unless they are in the list */
	TMap<FString, FPermissionListOwners> AllowList;

	/** List of owner names that requested all items to be filtered out */
	FPermissionListOwners DenyListAll;
	
	/** Triggered when filter changes */
	FSimpleMulticastDelegate OnFilterChangedDelegate;

	/** Temporarily prevent delegate from being triggered */
	bool bSuppressOnFilterChanged = false;

	/** Type of paths this list represent */
	EPathPermissionListType ListType;
};
