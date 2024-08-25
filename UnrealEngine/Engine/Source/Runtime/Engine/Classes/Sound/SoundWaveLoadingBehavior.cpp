// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundWaveLoadingBehavior.h"

#include "Audio.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/CommandLine.h"
#include "SoundClass.h"
#include "SoundCue.h"
#include "SoundWave.h"
#include "UObject/LinkerLoad.h"

#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(TXT) case TXT: return TEXT(#TXT);
#endif

const TCHAR* EnumToString(const ESoundWaveLoadingBehavior InCurrentState)
{
	switch(InCurrentState)
	{
		FOREACH_ENUM_ESOUNDWAVELOADINGBEHAVIOR(CASE_ENUM_TO_TEXT)
	} 
	return TEXT("Unknown");
}


#if WITH_EDITOR

static int32 SoundWaveLoadingBehaviorUtil_CacheAllOnStartup = 0;
FAutoConsoleVariableRef CVAR_SoundWaveLoadingBehaviorUtil_CacheAllOnStartup(
	TEXT("au.editor.SoundWaveOwnerLoadingBehaviorCacheOnStartup"),
	SoundWaveLoadingBehaviorUtil_CacheAllOnStartup,
	TEXT("Disables searching the asset registry on startup of the singleton. Otherwise it will incrementally fill cache"),
	ECVF_Default
);

static int32 SoundWaveLoadingBehaviorUtil_Enable = 1;
FAutoConsoleVariableRef CVAR_SoundWaveLoadingBehaviorUtil_Enable(
	TEXT("au.editor.SoundWaveOwnerLoadingBehaviorEnable"),
	SoundWaveLoadingBehaviorUtil_Enable,
	TEXT("Enables or disables the Soundwave owner loading behavior tagging"),
	ECVF_Default
);

class FSoundWaveLoadingBehaviorUtil : public ISoundWaveLoadingBehaviorUtil
{
public:
	FSoundWaveLoadingBehaviorUtil()	
		: AssetRegistry(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	{
		if (SoundWaveLoadingBehaviorUtil_CacheAllOnStartup)
		{
			CacheAllClassLoadingBehaviors();
		}
	}
	virtual ~FSoundWaveLoadingBehaviorUtil() override = default;

private:
	void CacheAllClassLoadingBehaviors()
	{
		const bool bAssetRegistryInStartup = AssetRegistry.IsSearchAsync() && AssetRegistry.IsLoadingAssets();
		if (!ensureMsgf(!bAssetRegistryInStartup,TEXT("Function must not be called until after cook has started and waited on the AssetRegistry already.")))
		{
			AssetRegistry.WaitForCompletion();
		}
		
		TArray<FAssetData> SoundClasses;
		AssetRegistry.GetAssetsByClass(USoundClass::StaticClass()->GetClassPathName(), SoundClasses, true);
		
		for (const FAssetData& i : SoundClasses)
		{
			LoadAndCacheClass(i);
		}
	}

	FClassData WalkClassHierarchy(USoundClass* InClass) const
	{
		FClassData Behavior(InClass);
		while (InClass->ParentClass && InClass->Properties.LoadingBehavior == ESoundWaveLoadingBehavior::Inherited)
		{
			InClass = InClass->ParentClass;
			if (InClass->HasAnyFlags(RF_NeedLoad))
			{
				InClass->GetLinker()->Preload(InClass);
			}
			if (InClass->HasAnyFlags(RF_NeedPostLoad))
			{
				InClass->ConditionalPostLoad();
			}
			Behavior = FClassData(InClass);
		}
		
		// If we failed to find anything other than inherited, use the default which is cvar'd.
		if (Behavior.LoadingBehavior == ESoundWaveLoadingBehavior::Inherited ||
			Behavior.LoadingBehavior == ESoundWaveLoadingBehavior::Uninitialized )
		{
			Behavior.LoadingBehavior = USoundWave::GetDefaultLoadingBehavior();
			Behavior.LengthOfFirstChunkInSeconds = 0;
		}
		return Behavior;
	}

	FClassData LoadAndCacheClass(const FAssetData& InAssetData) const
	{
		if (USoundClass* SoundClass = Cast<USoundClass>(InAssetData.GetAsset()))
		{
			FClassData Result = WalkClassHierarchy(SoundClass);
			CacheClassLoadingBehaviors.Add(InAssetData.PackageName,Result);
			return Result;
		}
		return {};
	}
	
	virtual FClassData FindOwningLoadingBehavior(const USoundWave* InWave, const ITargetPlatform* InTargetPlatform) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindOwningLoadingBehavior);

		// This code: Given a wave, finds all cues that references it. (reverse lookup)
		// Then finds the SoundClasses those cues use, traverses the heirarchy (from lookup) to determine the loading behavior.
		// Then stack ranks the most important behavior. (RetainOnLoad (Highest), PrimeOnLoad (Medium), LoadOnDemand (Lowest))
		// Which ever wins, we also capture the "SizeOfFirstChunk" to use for that wave.

		if (!InWave)
		{
			return {};
		}

		const bool bIsAssetRegistryStartup = AssetRegistry.IsSearchAsync() && AssetRegistry.IsLoadingAssets();

		// Disallow during startup of registry (cookers will have already done this)
		if (bIsAssetRegistryStartup)
		{
			UE_LOG(LogAudio, Warning, TEXT("FindOwningLoadingBehavior called before AssetRegistry is ready. SoundWave=%s"), *InWave->GetName());
			return {};
		}
	
		const UPackage* WavePackage = InWave->GetPackage();
		if (!WavePackage)
		{
			return {};
		}

		TArray<FName> SoundWaveReferencerNames;
		if (!AssetRegistry.GetReferencers(WavePackage->GetFName(), SoundWaveReferencerNames))
		{
			return {};
		}

		if (SoundWaveReferencerNames.IsEmpty())
		{
			return {};
		}

		// Filter on SoundCues.
		FARFilter Filter;
		// Don't rely on the AR to filter out classes as it's going to gather all assets for the specified classes first, then filtering for the provided packages names afterward,
		// resulting in execution time being over 100 times slower than filtering for classes after package names.
		//Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
		//Filter.bRecursiveClasses = true;
		Filter.PackageNames = SoundWaveReferencerNames;
		TArray<FAssetData> ReferencingSoundCueAssetDataArray;
		if (!AssetRegistry.GetAssets(Filter, ReferencingSoundCueAssetDataArray))
		{
			return {};
		}

		// Filter out unwanted classes, see above comment for details.
		for (int32 AssetIndex = 0; AssetIndex < ReferencingSoundCueAssetDataArray.Num(); AssetIndex++)
		{
			if (UClass* AssetClass = ReferencingSoundCueAssetDataArray[AssetIndex].GetClass(); !AssetClass || !AssetClass->IsChildOf<USoundCue>())
			{
				ReferencingSoundCueAssetDataArray.RemoveAtSwap(AssetIndex--);
			}
		}

		if (ReferencingSoundCueAssetDataArray.IsEmpty())
		{
			return {};
		}

		FClassData MostImportantLoadingBehavior;
		for (const FAssetData& CueAsset : ReferencingSoundCueAssetDataArray)
		{
			// Query for class references from the Cue instead of loading and opening it.
			TArray<FName> SoundCueReferences;
			if (!AssetRegistry.GetDependencies(CueAsset.PackageName, SoundCueReferences))
			{
				UE_LOG(LogAudio, Warning, TEXT("Failed to query SoundCue '%s' for it's dependencies."), 
				       *CueAsset.PackagePath.ToString() );
				continue;
			}

			if (SoundCueReferences.Num() == 0)
			{
				continue;
			}

			// Filter for Classes.
			FARFilter ClassFilter;
			ClassFilter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
			ClassFilter.PackageNames = SoundCueReferences;
			TArray<FAssetData> ReferencedSoundClasses;
			if (!AssetRegistry.GetAssets(ClassFilter, ReferencedSoundClasses))
			{
				UE_LOG(LogAudio, Warning, TEXT("Failed to filter for Soundclasses from the SoundCue dependencies for '%s'"), *CueAsset.PackagePath.ToString());
				continue;
			}			
			
			// If there's more than one, rank them.
			for (const FAssetData& Class : ReferencedSoundClasses)
			{
				FClassData CacheLoadingBehavior;
				FScopeLock Lock(&CacheCS);
				if (const FClassData* Found = CacheClassLoadingBehaviors.Find(Class.PackageName))
				{
					CacheLoadingBehavior = *Found;
				}
				else
				{
					CacheLoadingBehavior = LoadAndCacheClass(Class);
				}

				// Compare if this is more important
				if (MostImportantLoadingBehavior.CompareGreater(CacheLoadingBehavior, InTargetPlatform))
				{
					MostImportantLoadingBehavior = CacheLoadingBehavior;
				}
			}
		}
		
		// Return the most important one we found.
		return MostImportantLoadingBehavior;
	}
	
	// Make cache here.
	IAssetRegistry& AssetRegistry;
	mutable TMap<FName, FClassData> CacheClassLoadingBehaviors;
	mutable FCriticalSection CacheCS;
};

ISoundWaveLoadingBehaviorUtil* ISoundWaveLoadingBehaviorUtil::Get()
{
	// Cvar disable system if necessary
	if (!SoundWaveLoadingBehaviorUtil_Enable)
	{
		return nullptr;
	}
		
	// Only run while the cooker is active.
	if (!IsRunningCookCommandlet())
	{		
		return nullptr;
	}
		
	static FSoundWaveLoadingBehaviorUtil Instance;
	return &Instance;
}

ISoundWaveLoadingBehaviorUtil::FClassData::FClassData(const USoundClass* InClass)
	: LoadingBehavior(InClass->Properties.LoadingBehavior)
	, LengthOfFirstChunkInSeconds(InClass->Properties.SizeOfFirstAudioChunkInSeconds)
{
}

bool ISoundWaveLoadingBehaviorUtil::FClassData::CompareGreater(const FClassData& InOther, const ITargetPlatform* InPlatform) const
{
	if (InOther.LoadingBehavior != LoadingBehavior)
	{
		// If loading behavior enum is less, it's more important.
		return InOther.LoadingBehavior < LoadingBehavior;
	}
	else 
	{
		// If we are using Prime/Retain, use one with the higher Length.
		if (LoadingBehavior == ESoundWaveLoadingBehavior::RetainOnLoad ||
			LoadingBehavior == ESoundWaveLoadingBehavior::PrimeOnLoad )
		{
			const float Length = LengthOfFirstChunkInSeconds.GetValueForPlatform(*InPlatform->PlatformName());
			const float OtherLength = InOther.LengthOfFirstChunkInSeconds.GetValueForPlatform(*InPlatform->PlatformName());
				
			if (OtherLength > Length)
			{
				return true;
			}
		}
		return false;
	}
}

#endif //WITH_EDITOR
