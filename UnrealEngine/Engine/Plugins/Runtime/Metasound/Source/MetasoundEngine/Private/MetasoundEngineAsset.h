// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metasound.h"
#include "MetasoundAssetManager.h"
#include "MetasoundUObjectRegistry.h"
#include "Serialization/Archive.h"

#if WITH_EDITORONLY_DATA
#include "MetasoundFrontendRegistries.h"
#include "Algo/Transform.h"
#endif // WITH_EDITORONLY_DATA


namespace Metasound
{
	/** MetaSound Engine Asset helper provides routines for UObject based MetaSound assets. 
	 * Any UObject deriving from FMetaSoundAssetBase should use these helper functions
	 * in their UObject overrides. 
	 */
	struct FMetaSoundEngineAssetHelper
	{
#if WITH_EDITOR
		template <typename TMetaSoundObject>
		static void PostEditUndo(TMetaSoundObject& InMetaSound)
		{
			InMetaSound.GetModifyContext().SetForceRefreshViews();
			if (UMetasoundEditorGraphBase* Graph = Cast<UMetasoundEditorGraphBase>(InMetaSound.GetGraph()))
			{
				Graph->RegisterGraphWithFrontend();
			}
		}

		template<typename TMetaSoundObject>
		static void SetReferencedAssetClasses(TMetaSoundObject& InMetaSound, TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses)
		{
			using namespace Metasound::Frontend;
			
			InMetaSound.ReferencedAssetClassKeys.Reset();
			InMetaSound.ReferencedAssetClassObjects.Reset();

			for (const IMetaSoundAssetManager::FAssetInfo& AssetClass : InAssetClasses)
			{
				InMetaSound.ReferencedAssetClassKeys.Add(AssetClass.RegistryKey);
				if (UObject* Object = AssetClass.AssetPath.TryLoad())
				{
					InMetaSound.ReferencedAssetClassObjects.Add(Object);
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to load referenced asset %s from asset %s"), *AssetClass.AssetPath.ToString(), *InMetaSound.GetPathName());
				}
			}
		}
#endif // WITH_EDITOR

		template <typename TMetaSoundObject>
		static TArray<FMetasoundAssetBase*> GetReferencedAssets(TMetaSoundObject& InMetaSound)
		{
			TArray<FMetasoundAssetBase*> ReferencedAssets;

			IMetasoundUObjectRegistry& UObjectRegistry = IMetasoundUObjectRegistry::Get();

			for (TObjectPtr<UObject>& Object : InMetaSound.ReferencedAssetClassObjects)
			{
				if (FMetasoundAssetBase* Asset = UObjectRegistry.GetObjectAsAssetBase(Object))
				{
					ReferencedAssets.Add(Asset);
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Referenced asset \"%s\", referenced from \"%s\", is not convertible to FMetasoundAssetBase"), *Object->GetPathName(), *InMetaSound.GetPathName());
				}
			}

			return ReferencedAssets;
		}

		template <typename TMetaSoundObject>
		static void PreSaveAsset(TMetaSoundObject& InMetaSound, FObjectPreSaveContext InSaveContext)
		{
#if WITH_EDITORONLY_DATA
			using namespace Metasound::Frontend;

			if (UMetasoundEditorGraphBase* MetaSoundGraph = Cast<UMetasoundEditorGraphBase>(InMetaSound.GetGraph()))
			{
				// Cooked data must be deterministic, so do not call register graph as this can
				// initiate an auto-update and/or local registry data cache and modify serialized data.
				if (!InSaveContext.IsCooking())
				{
					MetaSoundGraph->RegisterGraphWithFrontend();
					MetaSoundGraph->GetModifyContext().SetForceRefreshViews();
				}
			}

			// Do not call asset manager on CDO objects which may be loaded before asset 
			// manager is set.
			if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
			{
				AssetManager->WaitUntilAsyncLoadReferencedAssetsComplete(InMetaSound);
			}
#endif // WITH_EDITORONLY_DATA
		}

		template <typename TMetaSoundObject>
		static void SerializeToArchive(TMetaSoundObject& InMetaSound, FArchive& InArchive)
		{
			if (InArchive.IsLoading())
			{
				if (InMetaSound.VersionAsset())
				{
#if WITH_EDITORONLY_DATA
					if (UMetasoundEditorGraphBase* MetaSoundGraph = Cast<UMetasoundEditorGraphBase>(InMetaSound.GetGraph()))
					{
						MetaSoundGraph->SetVersionedOnLoad();
					}
#endif // WITH_EDITORONLY_DATA
				}
			}
		}

		template<typename TMetaSoundObject>
		static void PostLoad(TMetaSoundObject& InMetaSound)
		{
			using namespace Frontend;
			// Do not call asset manager on CDO objects which may be loaded before asset 
			// manager is set.
			const bool bIsCDO = InMetaSound.HasAnyFlags(RF_ClassDefaultObject);
			if (!bIsCDO)
			{
				if (InMetaSound.GetAsyncReferencedAssetClassPaths().Num() > 0)
				{
					if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
					{
						AssetManager->RequestAsyncLoadReferencedAssets(InMetaSound);
					}
					else
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Request for to load async references ignored from asset %s. Likely due loading before the MetaSoundEngine module."), *InMetaSound.GetPathName());
					}
				}
			}
		}

		template<typename TMetaSoundObject>
		static void OnAsyncReferencedAssetsLoaded(TMetaSoundObject& InMetaSound, const TArray<FMetasoundAssetBase*>& InAsyncReferences)
		{
			for (FMetasoundAssetBase* AssetBase : InAsyncReferences)
			{
				if (AssetBase)
				{
					if (UObject* OwningAsset = AssetBase->GetOwningAsset())
					{
						InMetaSound.ReferencedAssetClassObjects.Add(OwningAsset);
						InMetaSound.ReferenceAssetClassCache.Remove(FSoftObjectPath(OwningAsset));
					}
				}
			}
		}

#if WITH_EDITORONLY_DATA
		template <typename TMetaSoundObject>
		static void SetMetaSoundRegistryAssetClassInfo(TMetaSoundObject& InMetaSound, const Metasound::Frontend::FNodeClassInfo& InClassInfo)
		{
			using namespace Metasound;
			using namespace Metasound::Frontend;

			check(AssetTags::AssetClassID == GET_MEMBER_NAME_CHECKED(TMetaSoundObject, AssetClassID));
			check(AssetTags::IsPreset == GET_MEMBER_NAME_CHECKED(TMetaSoundObject, bIsPreset));
			check(AssetTags::RegistryInputTypes == GET_MEMBER_NAME_CHECKED(TMetaSoundObject, RegistryInputTypes));
			check(AssetTags::RegistryOutputTypes == GET_MEMBER_NAME_CHECKED(TMetaSoundObject, RegistryOutputTypes));
			check(AssetTags::RegistryVersionMajor == GET_MEMBER_NAME_CHECKED(TMetaSoundObject, RegistryVersionMajor));
			check(AssetTags::RegistryVersionMinor == GET_MEMBER_NAME_CHECKED(TMetaSoundObject, RegistryVersionMinor));

			bool bMarkDirty = InMetaSound.AssetClassID != InClassInfo.AssetClassID;
			bMarkDirty |= InMetaSound.RegistryVersionMajor != InClassInfo.Version.Major;
			bMarkDirty |= InMetaSound.RegistryVersionMinor != InClassInfo.Version.Minor;
			bMarkDirty |= InMetaSound.bIsPreset != InClassInfo.bIsPreset;

			InMetaSound.AssetClassID = InClassInfo.AssetClassID;
			InMetaSound.RegistryVersionMajor = InClassInfo.Version.Major;
			InMetaSound.RegistryVersionMinor = InClassInfo.Version.Minor;
			InMetaSound.bIsPreset = InClassInfo.bIsPreset;

			{
				TArray<FString> InputTypes;
				Algo::Transform(InClassInfo.InputTypes, InputTypes, [](const FName& Name) { return Name.ToString(); });

				const FString TypeString = FString::Join(InputTypes, *AssetTags::ArrayDelim);
				bMarkDirty |= InMetaSound.RegistryInputTypes != TypeString;
				InMetaSound.RegistryInputTypes = TypeString;
			}

			{
				TArray<FString> OutputTypes;
				Algo::Transform(InClassInfo.OutputTypes, OutputTypes, [](const FName& Name) { return Name.ToString(); });

				const FString TypeString = FString::Join(OutputTypes, *AssetTags::ArrayDelim);
				bMarkDirty |= InMetaSound.RegistryOutputTypes != TypeString;
				InMetaSound.RegistryOutputTypes = TypeString;
			}
		}
#endif // WITH_EDITORONLY_DATA
	};
} // namespace Metasound

