// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudioContextManager.h"
#include "OculusAmbisonicSpatializer.h"
#include "OculusAudioReverb.h"
#include "Features/IModularFeatures.h"
#include "AudioMixerDevice.h"
#include "IOculusAudioPlugin.h"

ovrAudioContext FOculusAudioContextManager::SerializationContext = nullptr;
UActorComponent* FOculusAudioContextManager::SerializationParent = nullptr;

FCriticalSection FOculusAudioContextManager::SerializationContextLock;

TMap<const FAudioDevice*, ovrAudioContext> FOculusAudioContextManager::ContextMap;
FCriticalSection FOculusAudioContextManager::ContextCritSection;

FOculusAudioContextManager::FOculusAudioContextManager()
{
	// empty
}

FOculusAudioContextManager::~FOculusAudioContextManager()
{
	SerializationContext = nullptr;
}

void FOculusAudioContextManager::OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
{
}

void FOculusAudioContextManager::OnListenerShutdown(FAudioDevice* AudioDevice)
{
	FOculusAudioPlugin* Plugin = &FModuleManager::GetModuleChecked<FOculusAudioPlugin>("OculusAudio");
	const FString OculusSpatializerPluginName = Plugin->GetSpatializationPluginFactory()->GetDisplayName();
	const FString CurrentSpatializerPluginName = AudioPluginUtilities::GetDesiredPluginName(EAudioPlugin::SPATIALIZATION);
	if (CurrentSpatializerPluginName.Equals(OculusSpatializerPluginName)) // we have a match!
	{
		OculusAudioSpatializationAudioMixer* Spatializer =
			static_cast<OculusAudioSpatializationAudioMixer*>(AudioDevice->GetSpatializationPluginInterface().Get());

		Spatializer->ClearContext();
	}

	const FString OculusReverbPluginName = Plugin->GetReverbPluginFactory()->GetDisplayName();
	const FString CurrentReverbPluginName = AudioPluginUtilities::GetDesiredPluginName(EAudioPlugin::REVERB);
	if (CurrentReverbPluginName.Equals(OculusReverbPluginName))
	{
		OculusAudioReverb* Reverb = static_cast<OculusAudioReverb*>(AudioDevice->ReverbPluginInterface.Get());
		Reverb->ClearContext();
	}

	// FIXME:
	// There's a possibility this will leak if a Oculus Binaural submix is created,
	// but Oculus audio is not specified as the spatialization or reverb plugin.
	// This is a niche use case, but could be solved by having the oculus soundfield explicitly destroy
	// a context.
	DestroyContextForAudioDevice(AudioDevice);
	SerializationContext = nullptr;
}

ovrAudioContext FOculusAudioContextManager::GetOrCreateSerializationContext(UActorComponent* Parent)
{
	FScopeLock ScopeLock(&SerializationContextLock);

#if 0 //WITH_EDITOR
	/* NOTE: this was meant to be some magic that would clean up the serialization 
	context so there is no stale state in PIE. This failed when there was no geometry 
	in the scene. Since the plugins are force to use one global context that seems 
	like a better option for the vanilla UE integraiton to do as well for consistency.
	*/
	if (SerializationParent != nullptr)
	{
		AActor* Actor = SerializationParent->GetOwner();
		if (Actor != nullptr)
		{
			UWorld* World = Actor->GetWorld();
			if (World != nullptr)
			{
				if (World->WorldType == EWorldType::Editor)
				{
					// It was just an editor serialize, we don't need these geometry objects
					OVRA_CALL(ovrAudio_DestroyContext)(SerializationContext);
					SerializationContext = nullptr;
					SerializationParent = nullptr;
				}
			}
		}
	}
#endif

	ovrAudioContext PluginContext = FOculusAudioLibraryManager::Get().GetPluginContext();
	if (PluginContext != nullptr)
	{
		return PluginContext;
	}

	if (SerializationContext == nullptr)
	{
		ovrResult Result = OVRA_CALL(ovrAudio_CreateContext)(&SerializationContext, nullptr);
		if (Result != ovrSuccess)
		{
			const TCHAR* ErrString = GetOculusErrorString(Result);
			UE_LOG(LogAudio, Error, TEXT("Oculus Audio SDK Error - %s: %s"), TEXT("Failed to create Oculus Audio context for serialization"), ErrString);
			return nullptr;
		}

		SerializationParent = Parent;
	}

	return SerializationContext;
}

ovrAudioContext FOculusAudioContextManager::GetContextForAudioDevice(const FAudioDevice* InAudioDevice)
{
	FScopeLock ScopeLock(&ContextCritSection);
	ovrAudioContext* Context = ContextMap.Find(InAudioDevice);
	if (Context)
	{
		return *Context;
	}
	else
	{
		return nullptr;
	}
}

ovrAudioContext FOculusAudioContextManager::CreateContextForAudioDevice(FAudioDevice* InAudioDevice)
{
	ovrAudioContextConfiguration ContextConfig = { 0 };
	ContextConfig.acc_BufferLength = InAudioDevice->GetBufferLength();
	ContextConfig.acc_MaxNumSources = InAudioDevice->GetMaxSources();
	ContextConfig.acc_SampleRate = InAudioDevice->GetSampleRate();
	ContextConfig.acc_Size = sizeof(ovrAudioContextConfiguration);

	ovrAudioContext NewContext = nullptr;
	ovrResult Result = OVRA_CALL(ovrAudio_CreateContext)(&NewContext, &ContextConfig);

	if(ensure(Result == ovrSuccess))
	{
		FScopeLock ScopeLock(&ContextCritSection);
		return ContextMap.Add(InAudioDevice, NewContext);
	}
	else
	{
		return nullptr;
	}
}

void FOculusAudioContextManager::DestroyContextForAudioDevice(const FAudioDevice* InAudioDevice)
{
	FScopeLock ScopeLock(&ContextCritSection);
	ovrAudioContext* Context = ContextMap.Find(InAudioDevice);

	if (Context)
	{
		OVRA_CALL(ovrAudio_DestroyContext)(*Context);
		ContextMap.Remove(InAudioDevice);
	}
}
