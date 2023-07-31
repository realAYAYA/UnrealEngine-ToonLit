// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioPluginUtilities.h"
#include "OVR_Audio.h"

class UActorComponent;
class FOculusAudioContextManager : public IAudioPluginListener
{
public:
	FOculusAudioContextManager();
	virtual ~FOculusAudioContextManager() override;

	virtual void OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld) override;
	virtual void OnListenerShutdown(FAudioDevice* AudioDevice) override;

	static ovrAudioContext GetOrCreateSerializationContext(UActorComponent* Parent);

	// Returns an ovrAudioContext for the given audio device, or nullptr if one does not exist.
	static ovrAudioContext GetContextForAudioDevice(const FAudioDevice* InAudioDevice);

	// Creates a new ovrAudioContext for a given audio device.
	// the InAudioDevice ptr is no longer used for anything besides looking up contexts after this call is completed.
	static ovrAudioContext CreateContextForAudioDevice(FAudioDevice* InAudioDevice);
	static void DestroyContextForAudioDevice(const FAudioDevice* InAudioDevice);

private:
	// FIXME: can we do something better than global static variables?
	static ovrAudioContext SerializationContext;
	static FCriticalSection SerializationContextLock;

	static UActorComponent* SerializationParent;

	static TMap<const FAudioDevice*, ovrAudioContext> ContextMap;
	static FCriticalSection ContextCritSection;
};