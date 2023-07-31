// Copyright Epic Games, Inc. All Rights Reserved.

#include "FoliageSupport/InstancedFoliageActorData.h"

#include "FoliageType.h"
#include "FoliageRestorationInfo.h"
#include "LevelSnapshotsLog.h"
#include "Selection/PropertySelectionMap.h"

#include "InstancedFoliageActor.h"

namespace UE::LevelSnapshots::Foliage::Private::Internal
{
	static void SaveAsset(FArchive& Archive, UFoliageType* FoliageType, FFoliageInfo& FoliageInfo, TMap<TSoftObjectPtr<UFoliageType>, FFoliageInfoData>& FoliageAssets)
	{
		FFoliageInfoData& InfoData = FoliageAssets.Add(FoliageType);
		InfoData.Save(Archive, FoliageInfo);
	}
	
	static void SaveSubobject(FArchive& Archive, UFoliageType* FoliageType, FFoliageInfo& FoliageInfo,TArray<FSubobjectFoliageInfoData>& SubobjectData)
	{
		FSubobjectFoliageInfoData Data;
		Data.Save(Archive, FoliageType, FoliageInfo);
		SubobjectData.Emplace(Data);
	}
}


FArchive& UE::LevelSnapshots::Foliage::Private::FInstancedFoliageActorData::SerializeInternal(FArchive& Ar)
{
	Ar << FoliageAssets;
	Ar << SubobjectData;
	return Ar;
}

void UE::LevelSnapshots::Foliage::Private::FInstancedFoliageActorData::Save(FArchive& Archive, AInstancedFoliageActor* FoliageActor)
{
	FoliageActor->ForEachFoliageInfo([this, &Archive, FoliageActor](UFoliageType* FoliageType, FFoliageInfo& FoliageInfo)
	{
		FFoliageImpl* Impl = FoliageInfo.Implementation.Get();
		if (!FoliageType || !Impl)
		{
			return true;
		}
		
		const bool bIsSubobject = FoliageType->IsIn(FoliageActor);
		if (bIsSubobject)
		{
			Internal::SaveSubobject(Archive, FoliageType, FoliageInfo, SubobjectData);
		}
		else
		{
			Internal::SaveAsset(Archive, FoliageType, FoliageInfo, FoliageAssets);
		}
		
		return true;
	});
}

namespace UE::LevelSnapshots::Foliage::Private::Internal
{
	static FFoliageInfo* FindOrAddFoliageInfo(UFoliageType* FoliageType, AInstancedFoliageActor* FoliageActor)
	{
		FFoliageInfo* FoliageInfo = FoliageActor->FindInfo(FoliageType);
		if (!FoliageInfo)
		{
			FoliageInfo = &FoliageActor->AddFoliageInfo(FoliageType).Get();
		}

		UE_CLOG(FoliageInfo == nullptr, LogLevelSnapshots, Warning, TEXT("Failed to create foliage info for foliage type %s"), *FoliageType->GetPathName());
		return FoliageInfo; 
	}

	static void HandleExistingFoliageUsingRequiredComponent(AInstancedFoliageActor* FoliageActor, FName RequiredComponentName, const TMap<FName, UFoliageType*>& PreexistingComponentToFoliageType, UFoliageType* AllowedFoliageType)
	{
		// Handles the following case:
		// 1. Add (static mesh) foliage type FoliageA. Let's suppose the component that is added is called Component01.
		// 2. Take snapshot
		// 3. Reload map without saving
		// 4. Add foliage type FoliageB. This creates a new component that is also called Component01.
		// 5. Restore snapshot
		// 
		// Without the below check, both FoliageA and FoliageB would use Component01 to add instances.
		// We remove the foliage type under the "interpretation" that Component01 is "restored" to reuse the previous foliage type.
		if (UFoliageType* const* FoliageType = PreexistingComponentToFoliageType.Find(RequiredComponentName); FoliageType && *FoliageType != AllowedFoliageType)
		{
			TUniqueObj<FFoliageInfo> FoliageInfo;
			FoliageActor->RemoveFoliageInfoAndCopyValue(*FoliageType, FoliageInfo);
		}
	}

	static void RemoveRecreatedComponent(AInstancedFoliageActor* FoliageActor, TOptional<FName> RecreatedComponentName)
	{
		if (ensure(RecreatedComponentName))
		{
			for (UHierarchicalInstancedStaticMeshComponent* Component : TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*>(FoliageActor))
			{
				if (Component->GetFName() == *RecreatedComponentName)
				{
					Component->DestroyComponent();
					return;
				}
			}
		}
	}
	
	static void ApplyAssets(
		FArchive& Archive,
		AInstancedFoliageActor* FoliageActor,
		const FFoliageRestorationInfo& RestorationInfo,
		const TMap<TSoftObjectPtr<UFoliageType>, FFoliageInfoData>& FoliageAssets,
		const TMap<FName, UFoliageType*> PreexistingComponentToFoliageType)
	{
		// Some objects may not want to be restored (either due to errors or because user deselected them) ...
		int64 DataToSkip = 0;
		
		for (auto AssetIt = FoliageAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FFoliageInfoData& FoliageInfoData = AssetIt->Value;
			
			bool bWasRestored = false;
			ON_SCOPE_EXIT
			{
				if (!bWasRestored)
				{
					// Standard component restoration has already recreated the component. Remove it again.
					RemoveRecreatedComponent(FoliageActor, FoliageInfoData.GetComponentName());
					DataToSkip += FoliageInfoData.GetFoliageInfoArchiveSize();
					// TODO: Remove the foliage info from FoliageActor ... it was already serialized into FoliageInfos
				}
			};
			
			UFoliageType* FoliageType = AssetIt->Key.LoadSynchronous();
			if (!FoliageType)
			{
				UE_LOG(LogLevelSnapshots, Warning, TEXT("Foliage type %s is missing. Component %s will not be restored."), *AssetIt->Key.ToString(), *FoliageInfoData.GetComponentName().Get(NAME_None).ToString());
				continue;
			}

			if (RestorationInfo.ShouldSkipFoliageType(FoliageInfoData) || !RestorationInfo.ShouldSerializeFoliageType(FoliageInfoData))
			{
				continue;
			}

			FFoliageInfo* FoliageInfo = FindOrAddFoliageInfo(FoliageType, FoliageActor);
			if (FoliageInfo)
			{
				// If there were no instances, no component existed, hence no component name could be saved
				const bool bHadAtLeastOneInstanceWhenSaved = FoliageInfoData.GetComponentName().IsSet();
				if (bHadAtLeastOneInstanceWhenSaved)
				{
					HandleExistingFoliageUsingRequiredComponent(FoliageActor, *FoliageInfoData.GetComponentName(), PreexistingComponentToFoliageType, FoliageType);
				}

				// ... since Archive data is sequential we must account for possibly having skipped some data
				Archive.Seek(Archive.Tell() + DataToSkip);
				DataToSkip = 0;
				
				FoliageInfoData.ApplyTo(Archive, *FoliageInfo);
				bWasRestored = true;
			}
		}
	}

	static void ApplySubobjects(
		FArchive& Archive,
		AInstancedFoliageActor* FoliageActor,
		const FFoliageRestorationInfo& RestorationInfo,
		const TArray<FSubobjectFoliageInfoData>& SubobjectData, 
		const TMap<FName, UFoliageType*> PreexistingComponentToFoliageType)
	{
		// Some objects may not want to be restored (either due to errors or because user deselected them) ...
		int64 DataToSkip = 0;
		
		for (const FSubobjectFoliageInfoData& InstanceData : SubobjectData)
		{
			bool bWasRestored = false;
			bool bWasPartiallyRestored = false;
			ON_SCOPE_EXIT
			{
				if (!bWasRestored)
				{
					// Standard component restoration has already recreated the component. Remove it again.
					RemoveRecreatedComponent(FoliageActor, InstanceData.GetComponentName());
					DataToSkip += static_cast<uint64>(bWasPartiallyRestored) * InstanceData.GetFoliageTypeArchiveSize() + InstanceData.GetFoliageInfoArchiveSize();
					// TODO: Remove the foliage info from FoliageActor ... it was already serialized into FoliageInfos
				}
			};
			
			if (RestorationInfo.ShouldSkipFoliageType(InstanceData) || !RestorationInfo.ShouldSerializeFoliageType(InstanceData))
			{
				continue;
			}

			UFoliageType* FoliageType = InstanceData.FindOrRecreateSubobject(FoliageActor);
			if (!FoliageType)
			{
				UE_LOG(LogLevelSnapshots, Error, TEXT("Failed to recreate foliage type. Skipping..."));
				continue;
			}

			// ... since Archive data is sequential we must account for possibly having skipped some data
			Archive.Seek(Archive.Tell() + DataToSkip);
			DataToSkip = 0;

			bWasPartiallyRestored = true;
			if (!InstanceData.ApplyToFoliageType(Archive, FoliageType))
			{
				UE_LOG(LogLevelSnapshots, Warning, TEXT("Skipping restoration of foliage item due to errors."));
				continue;
			}
				
			FFoliageInfo* FoliageInfo = FindOrAddFoliageInfo(FoliageType, FoliageActor);
			if (FoliageInfo
				&& ensureAlwaysMsgf(InstanceData.GetComponentName(), TEXT("ComponentName is supposed to have been saved. Investigate. Maybe you used an unsupported foliage type (only EFoliageImplType::StaticMesh is supported)?")))
			{
				HandleExistingFoliageUsingRequiredComponent(FoliageActor, *InstanceData.GetComponentName(), PreexistingComponentToFoliageType, FoliageType);
				InstanceData.ApplyToFoliageInfo(Archive, *FoliageInfo);
				bWasRestored = true;
			}
		}
	}

	static TMap<FName, UFoliageType*> BuildComponentToFoliageType(AInstancedFoliageActor* FoliageActor)
	{
		TMap<FName, UFoliageType*> Result;
		for (auto It = FoliageActor->GetFoliageInfos().CreateConstIterator(); It; ++It)
		{
			if (UHierarchicalInstancedStaticMeshComponent* Component = It->Value->GetComponent())
			{
				Result.Add(Component->GetFName(), It->Key);
			}
		}
		return Result;
	}
}

void UE::LevelSnapshots::Foliage::Private::FInstancedFoliageActorData::ApplyTo(FArchive& Archive, AInstancedFoliageActor* FoliageActor, const FPropertySelectionMap& SelectedProperties, bool bWasRecreated) const
{
	const FFoliageRestorationInfo RestorationInfo = FFoliageRestorationInfo::From(FoliageActor, SelectedProperties, bWasRecreated);
	const TMap<FName, UFoliageType*> PreexistingComponentToFoliageType = Internal::BuildComponentToFoliageType(FoliageActor);
	Internal::ApplyAssets(Archive, FoliageActor, RestorationInfo, FoliageAssets, PreexistingComponentToFoliageType);
	Internal::ApplySubobjects(Archive, FoliageActor, RestorationInfo, SubobjectData, PreexistingComponentToFoliageType);
}
