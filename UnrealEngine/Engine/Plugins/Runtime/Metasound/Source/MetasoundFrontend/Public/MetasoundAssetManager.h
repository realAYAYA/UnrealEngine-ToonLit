// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundVertex.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Forward Declarations
class FMetasoundAssetBase;
class UEdGraph;
struct FMetasoundFrontendClassName;

namespace Metasound
{
	namespace Frontend
	{
		using FNodeRegistryKey = FString;

		namespace AssetTags
		{
			extern const FString METASOUNDFRONTEND_API ArrayDelim;

#if WITH_EDITORONLY_DATA
			extern const FName METASOUNDFRONTEND_API IsPreset;
#endif // WITH_EDITORONLY_DATA

			extern const FName METASOUNDFRONTEND_API AssetClassID;
			extern const FName METASOUNDFRONTEND_API RegistryVersionMajor;
			extern const FName METASOUNDFRONTEND_API RegistryVersionMinor;

#if WITH_EDITORONLY_DATA
			extern const FName METASOUNDFRONTEND_API RegistryInputTypes;
			extern const FName METASOUNDFRONTEND_API RegistryOutputTypes;
#endif // WITH_EDITORONLY_DATA
		} // namespace AssetTags

		struct METASOUNDFRONTEND_API FMetaSoundAssetRegistrationOptions
		{
			// If true, forces a re-register of this class (and all class dependencies
			// if the following option 'bRegisterDependencies' is enabled).
			bool bForceReregister = true;

			// If true, forces flag to resync all view (editor) data pertaining to the given asset(s) being registered.
			bool bForceViewSynchronization = true;

			// If true, recursively attempts to register dependencies. (TODO: Determine if this option should be removed.
			// Must validate that failed dependency updates due to auto-update for ex. being disabled is handled gracefully
			// at runtime.)
			bool bRegisterDependencies = true;

			// Attempt to auto-update (Only runs if class not registered or set to force re-register.
			// Will not respect being set to true if project-level MetaSoundSettings specify to not run auto-update.)
			bool bAutoUpdate = true;

			// If true, warnings will be logged if updating a node results in existing connections being discarded.
			bool bAutoUpdateLogWarningOnDroppedConnection = false;

#if WITH_EDITOR
			// Attempt to rebuild referenced classes (only run if class not registered or set to force re-register)
			bool bRebuildReferencedAssetClasses = true;
#endif
		};

		class METASOUNDFRONTEND_API IMetaSoundAssetManager
		{
			static IMetaSoundAssetManager* Instance;

		public:
			static void Set(IMetaSoundAssetManager& InInterface)
			{
				if (!InInterface.IsTesting())
				{
					check(!Instance);
				}
				Instance = &InInterface;
			}

			static IMetaSoundAssetManager* Get()
			{
				return Instance;
			}

			static IMetaSoundAssetManager& GetChecked()
			{
				check(Instance);
				return *Instance;
			}

			struct FAssetInfo
			{
				const Metasound::Frontend::FNodeRegistryKey RegistryKey;
				FSoftObjectPath AssetPath;

				FORCEINLINE friend bool operator==(const FAssetInfo& InLHS, const FAssetInfo& InRHS)
				{
					return (InLHS.RegistryKey == InRHS.RegistryKey) && (InLHS.AssetPath == InRHS.AssetPath);
				}

				FORCEINLINE friend uint32 GetTypeHash(const IMetaSoundAssetManager::FAssetInfo& InInfo)
				{
					return HashCombineFast(::GetTypeHash(InInfo.RegistryKey), GetTypeHash(InInfo.AssetPath));
				}
			};

			// Whether or not manager is being used to run tests or not (enabling instances to be reset without asserting.)
			virtual bool IsTesting() const { return false; }

#if WITH_EDITORONLY_DATA
			// Adds missing assets using the provided asset's local reference class cache. Used
			// to prime system from asset attempting to register prior to asset scan being complete.
			virtual void AddAssetReferences(FMetasoundAssetBase& InAssetBase) = 0;
#endif

			// Add or Update a MetaSound Asset's entry data
			virtual Metasound::Frontend::FNodeRegistryKey AddOrUpdateAsset(const UObject& InObject) = 0;

			// Whether or not the class is eligible for auto-update
			virtual bool CanAutoUpdate(const FMetasoundFrontendClassName& InClassName) const = 0;

			// Whether or not the asset manager has loaded the given asset
			virtual bool ContainsKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const = 0;

			// Returns path associated with the given key (null if key is not registered with the AssetManager or was not loaded from asset)
			virtual const FSoftObjectPath* FindObjectPathFromKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const = 0;

#if WITH_EDITOR
			// Generates all asset info associated with registered assets that are referenced by the provided asset's graph.
			virtual TSet<FAssetInfo> GetReferencedAssetClasses(const FMetasoundAssetBase& InAssetBase) const = 0;
#endif // WITH_EDITOR

			// Rescans settings for denied assets not to run reference auto-update against.
			virtual void RescanAutoUpdateDenyList() = 0;

			// Attempts to load an FMetasoundAssetBase from the given path, or returns it if its already loaded
			virtual FMetasoundAssetBase* TryLoadAsset(const FSoftObjectPath& InObjectPath) const = 0;

			// Returns asset associated with the given key (null if key is not registered with the AssetManager or was not loaded from asset)
			virtual FMetasoundAssetBase* TryLoadAssetFromKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const = 0;

			// Try to load referenced assets of the given asset or return them if they are already loaded (non-recursive).
			// @return - True if all referenced assets successfully loaded, false if not.
			virtual bool TryLoadReferencedAssets(const FMetasoundAssetBase& InAssetBase, TArray<FMetasoundAssetBase*>& OutReferencedAssets) const = 0;
			
			// Requests an async load of all async referenced assets of the input asset.
			virtual void RequestAsyncLoadReferencedAssets(FMetasoundAssetBase& InAssetBase) = 0;

			// Waits until all async load requests related to this asset are complete. 
 			virtual void WaitUntilAsyncLoadReferencedAssetsComplete(FMetasoundAssetBase& InAssetBase) = 0;
		};
	} // namespace Frontend
};
