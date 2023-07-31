//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "ISteamAudioModule.h"
#include "phonon.h"
#include "PhononCommon.h"
#include "PhononScene.h"

namespace SteamAudio
{
	/**
	 * Maps from UE FName identifiers stored on AudioComponents to a unique integer for the Phonon API.
	 * The expected number of baked sources for a given scene is quite low (< 1k usually), so this simply
	 * maps to random integers in (0, INT_MAX).
	 */
	class STEAMAUDIO_API FIdentifierMap
	{
	public:
		bool ContainsKey(const FString& Key) const;
		bool ContainsValue(const int32 Value) const;
		int32 Add(const FString& Identifier);
		void Add(const TPair<FString, int32>& Pair);
		int32 Get(const FString& Identifier) const;
		FString ToString() const;
		void FromString(const FString& MappingString);
		void Empty();

	private:
		TArray<TPair<FString, int32>> IdentifierMapping;
	};

	bool STEAMAUDIO_API LoadBakedIdentifierMapFromDisk(UWorld* World, FIdentifierMap& BakedIdentifierMap);
	bool STEAMAUDIO_API SaveBakedIdentifierMapToDisk(UWorld* World, const FIdentifierMap& BakedIdentifierMap);

	/**
	 * Handles an instance of the Steam Audio environment and the environmental renderer used by the audio plugins.
	 */
	class FEnvironment
	{
	public:
		FEnvironment();
		~FEnvironment();

		bool Initialize(UWorld* World, FAudioDevice* InAudioDevice, FString* CurrentLevelName);
		void Shutdown();

		IPLhandle GetScene() { return PhononScene; };
		IPLhandle GetEnvironment() const { return PhononEnvironment; };
		IPLhandle GetComputeDevice() const { return ComputeDevice; };
		IPLhandle GetEnvironmentalRenderer() const { return EnvironmentalRenderer; }
		const IPLSimulationSettings& GetSimulationSettings() const { return SimulationSettings; }
		const IPLRenderingSettings& GetRenderingSettings() const { return RenderingSettings; }
		const FIdentifierMap& GetBakedIdentifierMap() const { return BakedIdentifierMap; }
		FCriticalSection* GetEnvironmentCriticalSectionHandle() { return &EnvironmentCriticalSection; };

	private:
		FCriticalSection EnvironmentCriticalSection;

		IPLhandle ComputeDevice;
		IPLhandle PhononScene;
		IPLhandle PhononEnvironment;
		IPLhandle EnvironmentalRenderer;
		IPLhandle ProbeManager;
		TArray<IPLhandle> ProbeBatches;

		IPLSimulationSettings SimulationSettings;
		IPLRenderingSettings RenderingSettings;

		FPhononSceneInfo PhononSceneInfo;
		FIdentifierMap BakedIdentifierMap;
	};
}