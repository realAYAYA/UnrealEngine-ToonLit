//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "SteamAudioModule.h"
#include "PhononSpatialization.h"
#include "PhononOcclusion.h"
#include "PhononReverb.h"
#include "SteamAudioEnvironment.h"
#include "PhononCommon.h"
#include "AudioPluginUtilities.h"
#include "PhononGeometryComponent.h"

namespace SteamAudio
{
	/************************************************************************/
	/* FPhononPluginManager                                                 */
	/* This ListenerObserver owns the Steam Audio environment, and          */
	/* dispatches information to the Steam Audio reverb and occlusion       */
	/* plugins.                                                             */
	/************************************************************************/
	class FPhononPluginManager : public IAudioPluginListener
	{
	public:
		FPhononPluginManager();
		FPhononPluginManager(AActor* SubLevelRef);
		~FPhononPluginManager();

		//~ Begin IAudioPluginListener
		virtual void OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld) override;
		virtual void OnTick(UWorld* InWorld, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds) override;
		virtual void OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds) override;
		virtual void OnListenerShutdown(FAudioDevice* AudioDevice) override;
		virtual void OnWorldChanged(FAudioDevice* AudioDevice, UWorld* ListenerWorld) override;
		//~ End IAudioPluginListener

		/** Cache of dynamic geometry currently active in the level. UPROPERTY to prevent GC */
		UPROPERTY()
		static TArray<FDynamicGeometryMap> DynamicGeometry;

		/** Cache of dynamic geometry scenes currently active in the level. UPROPERTY to prevent GC */
		UPROPERTY()
		static TMap<FString, IPLhandle> DynamicSceneMap;

		/** Add dynamic geometry to runtime cache */
		static void AddDynamicGeometry(UPhononGeometryComponent* PhononGeometryComponent);

		/** Remove dynamic geometry to runtime cache */
		static void RemoveDynamicGeometry(UPhononGeometryComponent* PhononGeometryComponent);

		/** Return the current phonon environment */
		FEnvironment& GetPhononEnvironment();

		/** Used to identify this plugin from other listeners - better if IAudioPluginListener.h has a GetPluginType() function instead */
		static const FName PluginID;

	private:
		void InitializeEnvironment(FAudioDevice* AudioDevice, UWorld* ListenerWorld);

		bool bEnvironmentInitialized;
		FEnvironment Environment;
		FPhononReverb* ReverbPtr;
		FPhononOcclusion* OcclusionPtr;
		AActor* SubLevelReference = nullptr;

	private:
		/* Helper function for checking whether the user is using Steam Audio for spatialization, reverb, and/or occlusion: */
		static bool IsUsingSteamAudioPlugin(EAudioPlugin PluginType);
		static UWorld* CurrentLevel;

	};
}
