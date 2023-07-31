// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/SortedMap.h"

/**
 * Singleton cache of property tags data for each asset class.
 */
class CONTENTBROWSERASSETDATASOURCE_API FAssetPropertyTagCache
{
public:
	struct FPropertyTagCache
	{
		/** The kind of data represented by this tag value */
		UObject::FAssetRegistryTag::ETagType TagType = UObject::FAssetRegistryTag::TT_Hidden;

		/** Flags giving hints at how to display this tag value in the UI (see ETagDisplay) */
		uint32 DisplayFlags = UObject::FAssetRegistryTag::TD_None;

		/** Resolved display name of the associated tag */
		FText DisplayName;

		/** Optional tooltip of the associated tag */
		FText TooltipText;

		/** Optional suffix to apply to values of the tag attribute in the UI */
		FText Suffix;

		/** Optional value which denotes which values should be considered "important" in the UI */
		FString ImportantValue;
	};

	struct FClassPropertyTagCache
	{
		friend class FAssetPropertyTagCache;

	public:
		const FPropertyTagCache* GetCacheForTag(const FName InTagName) const
		{
			return TagNameToCachedDataMap.Find(InTagName);
		}

		/** See whether the given name is a known alias of a tag, and if so, return the real tag name */
		FName GetTagNameFromAlias(const FName InTagName) const
		{
			return DisplayNameToTagNameMap.FindRef(InTagName);
		}

	private:
		/** Map of an internal tag name to its cached data */
		TSortedMap<FName, FPropertyTagCache, FDefaultAllocator, FNameFastLess> TagNameToCachedDataMap;

		/** Map of a tag display name back to its real internal name */
		TSortedMap<FName, FName, FDefaultAllocator, FNameFastLess> DisplayNameToTagNameMap;
	};

	/** Get the singleton instance */
	static FAssetPropertyTagCache& Get();

	/** Get (or populate) the cache for the given asset class */
	const FClassPropertyTagCache& GetCacheForClass(FTopLevelAssetPath InClassName);

private:
	/** Mapping of the asset class name to its cache */
	TMap<FTopLevelAssetPath, TSharedPtr<FClassPropertyTagCache>> ClassToCacheMap;
};
