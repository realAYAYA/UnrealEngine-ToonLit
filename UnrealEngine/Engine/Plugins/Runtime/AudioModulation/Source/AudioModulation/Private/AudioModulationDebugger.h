// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Build.h"

#if WITH_AUDIOMODULATION
#if !UE_BUILD_SHIPPING
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "SoundModulationGenerator.h"
#include "SoundModulationProxy.h"
#include "Templates/SharedPointer.h"


namespace AudioModulation
{
	// Forward Declarations
	struct FReferencedProxies;

	struct FControlBusMixStageDebugInfo
	{
		float TargetValue;
		float CurrentValue;
	};

	struct FControlBusMixDebugInfo
	{
		FString Name;
		uint32 Id;
		uint32 RefCount;

		TMap<uint32, FControlBusMixStageDebugInfo> Stages;
	};

	struct FControlBusDebugInfo
	{
		FString Name;
		float DefaultValue;
		float GeneratorValue;
		float MixValue;
		float Value;
		uint32 Id;
		uint32 RefCount;
	};

	struct FGeneratorSort
	{
		FORCEINLINE bool operator()(const FString& A, const FString& B) const
		{
			return A < B;
		}
	};

	struct FGeneratorDebugInfo
	{
		FGeneratorDebugInfo()
		{
		}

		FGeneratorDebugInfo(const TArray<FString>& InCategories)
			: Categories(InCategories)
		{
		}

		bool bEnabled = false;
		TArray<FString> Categories;

		FString NameFilter;

		using FInstanceValues = TArray<FString>;
		TArray<FInstanceValues> FilteredInstances;
	};

	class FAudioModulationDebugger : public TSharedFromThis<FAudioModulationDebugger, ESPMode::ThreadSafe>
	{
	public:
		FAudioModulationDebugger();

		void UpdateDebugData(double InElapsed, const FReferencedProxies& InRefProxies);
		void SetDebugBusFilter(const FString* InNameFilter);
		void SetDebugMatrixEnabled(bool bInIsEnabled);
		void SetDebugMixFilter(const FString* InNameFilter);
		void SetDebugGeneratorsEnabled(bool bInIsEnabled);
		void SetDebugGeneratorFilter(const FString* InFilter);
		void SetDebugGeneratorTypeFilter(const FString* InFilter, bool bInIsEnabled);
		bool OnPostHelp(FCommonViewportClient& ViewportClient, const TCHAR* Stream);
		int32 OnRenderStat(FCanvas& Canvas, int32 X, int32 Y, const UFont& Font);
		bool OnToggleStat(FCommonViewportClient& ViewportClient, const TCHAR* Stream);

		void ResetGeneratorStats();

	private:
		uint8 bActive : 1;
		uint8 bShowRenderStatMix : 1;
		uint8 bShowGenerators : 1;
		uint8 bEnableAllGenerators : 1;

		TArray<FControlBusDebugInfo> FilteredBuses;
		TArray<FControlBusMixDebugInfo> FilteredMixes;

		using FGeneratorSortMap = TSortedMap<FString, FGeneratorDebugInfo, FDefaultAllocator, FGeneratorSort>;
		FGeneratorSortMap FilteredGeneratorsMap;

		TMap<FString, bool> RequestedGeneratorUpdate;

		FString BusStringFilter;
		FString GeneratorStringFilter;
		FString MixStringFilter;

		float ElapsedSinceLastUpdate;
	};

} // namespace AudioModulation
#endif // !UE_BUILD_SHIPPING
#endif // WITH_AUDIOMODULATION
