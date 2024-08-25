// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "Misc/Crc.h"
#include "GameFeatureVersePathMapperCommandlet.generated.h"

class FAssetRegistryState;

namespace GameFeatureVersePathMapper
{
	/** Case sensitive hashing function for TMap */
	template <typename ValueType>
	struct FCaseSensitiveKeyMapFuncs : BaseKeyFuncs<ValueType, FString, /*bInAllowDuplicateKeys*/false>
	{
		static FORCEINLINE const FString& GetSetKey(const TPair<FString, ValueType>& Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(const FString& A, const FString& B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}
		static FORCEINLINE uint32 GetKeyHash(const FString& Key)
		{
			return FCrc::StrCrc32(*Key);
		}
	};

	struct FGameFeaturePluginInfo
	{
		FString GfpUri;
		TArray<FName> Dependencies;
	};

	struct FGameFeatureVersePathLookup
	{
		TMap<FString, FName, FDefaultSetAllocator, GameFeatureVersePathMapper::FCaseSensitiveKeyMapFuncs<FName>> VersePathToGfpMap;
		TMap<FName, FGameFeaturePluginInfo> GfpInfoMap;
	};

	GAMEFEATURES_API FString GetVerseAppDomain();
	GAMEFEATURES_API FString GetAltVerseAppDomain();

	/**
	 * Finds plugin dependencies and returns them in dependency order (reverse topological sort order)
	 */
	class FDepthFirstGameFeatureSorter
	{
	private:
		enum class EVisitState : uint8
		{
			None,
			Visiting,
			Visited
		};

		const TMap<FName, FGameFeaturePluginInfo>& GfpInfoMap;
		TMap<FName, EVisitState> VisitedPlugins;

		bool bIncludeVirtualNodes = false;

		bool Visit(FName Plugin, TFunctionRef<void(FName, const FString&)> AddOutput);

	public:
		/**
		 * Constructor
		 * @Param InGfpInfoMap Map containing plugin dependencies (FDepthFirstGameFeatureSorter points to this map, it is not copied)
		 */
		FDepthFirstGameFeatureSorter(const TMap<FName, FGameFeaturePluginInfo>& InGfpInfoMap, bool bInIncludeVirtualNodes = false) 
			: GfpInfoMap(InGfpInfoMap) 
			, bIncludeVirtualNodes(bInIncludeVirtualNodes)
		{}

		// @TODO: Allow passing a callback to fetch dependencies?

		/**
		 * Find and sort all dependencies
		 * @Param GetNextRootPlugin function that iterates plugins in the root set, returning None after the final plugin in the set.
		 * @Param AddOutput callback to receive roots and dependencies, called in dependency order
		 * @Return false if there is an error or a cyclic dependency is discovered
		 */
		GAMEFEATURES_API bool Sort(TFunctionRef<FName()> GetNextRootPlugin, TFunctionRef<void(FName, const FString&)> AddOutput);

		/**
		 * Find and sort all dependencies
		 * @Param RootPlugins set of root plugins
		 * @Param AddOutput callback to receive roots and dependencies, called in dependency order
		 * @Return false if there is an error or a cyclic dependency is discovered
		 */
		GAMEFEATURES_API bool Sort(TConstArrayView<FName> RootPlugins, TFunctionRef<void(FName, const FString&)> AddOutput);
		bool Sort(FName RootPlugin, TFunctionRef<void(FName, const FString&)> AddOutput) { return Sort(MakeArrayView(&RootPlugin, 1), AddOutput); }

		/**
		 * Find and sort all dependencies
		 * @Param RootPlugins set of root plugins
		 * @Param OutPlugins roots and dependencies in dependency order
		 * @Return false if there is an error or a cyclic dependency is discovered
		 */
		GAMEFEATURES_API bool Sort(TConstArrayView<FName> RootPlugins, TArray<FName>& OutPlugins);
		bool Sort(FName RootPlugin, TArray<FName>& OutPlugins) { return Sort(MakeArrayView(&RootPlugin, 1), OutPlugins); }
	};

	/**
	 * Build a FGameFeatureVersePathLookup that can be used to map Verse paths to Game Feature URIs
	 * @Param TargetPlatform If set, uses the corresponding platform config
	 * @Param DevAR If set, use the specified development asset registry state instead of the global asset registry
	 * @Return false if there is an error or a cyclic dependency is discovered
	 */
	GAMEFEATURES_API TOptional<FGameFeatureVersePathLookup> BuildLookup(const ITargetPlatform* TargetPlatform = nullptr, const FAssetRegistryState* DevAR = nullptr);
}

UCLASS(config = Editor)
class UGameFeatureVersePathMapperCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& CmdLineParams) override;

	GAMEFEATURES_API static FString GetGameFeatureRootVersePath();
};
