// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudioMixer.h"
#include "OculusAudioSettings.h"
#include "OculusAudioSourceSettings.h"
#include "OculusAudioContextManager.h"
#include "IOculusAudioPlugin.h"
#include "Stats/Stats.h"


float dbToLinear(float db)
{
	return powf(10.0f, db / 20.0f);
}

OculusAudioSpatializationAudioMixer::OculusAudioSpatializationAudioMixer()
	: Context(nullptr)
{
}

OculusAudioSpatializationAudioMixer::~OculusAudioSpatializationAudioMixer()
{
}

void OculusAudioSpatializationAudioMixer::ClearContext()
{
	Context = nullptr;
}

void OculusAudioSpatializationAudioMixer::Initialize(const FAudioPluginInitializationParams InitializationParams)
{
	FScopeLock ScopeLock(&ContextLock);
	InitParams = InitializationParams;

	Context = FOculusAudioContextManager::GetContextForAudioDevice(InitializationParams.AudioDevicePtr);
	if (!Context)
	{
		Context = FOculusAudioContextManager::CreateContextForAudioDevice(InitializationParams.AudioDevicePtr);
		check(Context);
	}

	Params.AddDefaulted(InitParams.NumSources);

	// UE is in centimeters
	ovrResult Result = OVRA_CALL(ovrAudio_SetUnitScale)(Context, 0.01f);
	OVR_AUDIO_CHECK(Result == ovrSuccess, "Failed to set unit scale");

	const UOculusAudioSettings* Settings = GetDefault<UOculusAudioSettings>();
	ApplyOculusAudioSettings(Settings);

	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &OculusAudioSpatializationAudioMixer::Tick));
}

void OculusAudioSpatializationAudioMixer::ApplyOculusAudioSettings(const UOculusAudioSettings* Settings)
{
	FScopeLock ScopeLock(&ContextLock);
	ovrResult Result = OVRA_CALL(ovrAudio_Enable)(Context, ovrAudioEnable_SimpleRoomModeling, Settings->EarlyReflections);
	OVR_AUDIO_CHECK(Result, "Failed to enable reflections");

	Result = OVRA_CALL(ovrAudio_Enable)(Context, ovrAudioEnable_LateReverberation, Settings->LateReverberation);
	OVR_AUDIO_CHECK(Result, "Failed to enable reverb");

	ovrAudioBoxRoomParameters Room = { 0 };
	Room.brp_Size = sizeof(Room);
	Room.brp_Width = Settings->Width;
	Room.brp_Height = Settings->Height;
	Room.brp_Depth = Settings->Depth;
	Room.brp_ReflectLeft = Settings->ReflectionCoefLeft;
	Room.brp_ReflectRight = Settings->ReflectionCoefRight;
	Room.brp_ReflectUp = Settings->ReflectionCoefUp;
	Room.brp_ReflectDown = Settings->ReflectionCoefDown;
	Room.brp_ReflectBehind = Settings->ReflectionCoefBack;
	Room.brp_ReflectFront = Settings->ReflectionCoefFront;

	Result = OVRA_CALL(ovrAudio_SetSimpleBoxRoomParameters)(Context, &Room);
	OVR_AUDIO_CHECK(Result, "Failed to set room parameters");

	Result = OVRA_CALL(ovrAudio_SetSharedReverbWetLevel)(Context, dbToLinear(Settings->ReverbWetLevel));
	OVR_AUDIO_CHECK(Result, "Failed to set room parameters");
}

void OculusAudioSpatializationAudioMixer::Shutdown()
{
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}
}

bool OculusAudioSpatializationAudioMixer::IsSpatializationEffectInitialized() const
{
	return Context != nullptr;
}

void OculusAudioSpatializationAudioMixer::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings)
{
	FScopeLock ScopeLock(&ContextLock);
	if (Context == nullptr)
	{
		UE_LOG(LogAudio, Error, TEXT("Oculus Audio Error - Context uninitialized. Sound %s (id=%d) will not play!"), *AudioComponentUserId.ToString(), SourceId);
		return;
	}

	if (InSettings != nullptr) 
	{
		UOculusAudioSourceSettings* Settings = CastChecked<UOculusAudioSourceSettings>(InSettings);

		uint32 Flags = 0;
		if (!Settings->EarlyReflectionsEnabled)
			Flags |= ovrAudioSourceFlag_ReflectionsDisabled;

		ovrResult Result = OVRA_CALL(ovrAudio_SetAudioSourceFlags)(Context, SourceId, Flags);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source flags");

		ovrAudioSourceAttenuationMode mode = Settings->AttenuationEnabled ? ovrAudioSourceAttenuationMode_InverseSquare : ovrAudioSourceAttenuationMode_None;
		Result = OVRA_CALL(ovrAudio_SetAudioSourceAttenuationMode)(Context, SourceId, mode, 1.0f);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source attenuation mode");

		Result = OVRA_CALL(ovrAudio_SetAudioSourceRange)(Context, SourceId, Settings->AttenuationRangeMinimum, Settings->AttenuationRangeMaximum);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source attenuation range");

		Result = OVRA_CALL(ovrAudio_SetAudioSourceRadius)(Context, SourceId, Settings->VolumetricRadius);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source volumetric radius");

		Result = OVRA_CALL(ovrAudio_SetAudioReverbSendLevel)(Context, SourceId, dbToLinear(Settings->ReverbSendLevel));
		OVR_AUDIO_CHECK(Result, "Failed to set reverb send level");
	}
}

void OculusAudioSpatializationAudioMixer::SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams)
{
	Params[VoiceId] = InParams;
}

void OculusAudioSpatializationAudioMixer::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
{
	FScopeLock ScopeLock(&ContextLock);
	if (InputData.SpatializationParams && Context)
	{
		Params[InputData.SourceId] = *InputData.SpatializationParams;

		// Translate the input position to OVR coordinates
		FVector OvrListenerPosition = ToOVRVector(Params[InputData.SourceId].ListenerPosition);
		FVector OvrListenerForward = ToOVRVector(Params[InputData.SourceId].ListenerOrientation.GetForwardVector());
		FVector OvrListenerUp = ToOVRVector(Params[InputData.SourceId].ListenerOrientation.GetUpVector());

		ovrResult Result = OVRA_CALL(ovrAudio_SetListenerVectors)(Context,
			OvrListenerPosition.X, OvrListenerPosition.Y, OvrListenerPosition.Z,
			OvrListenerForward.X, OvrListenerForward.Y, OvrListenerForward.Z,
			OvrListenerUp.X, OvrListenerUp.Y, OvrListenerUp.Z);
		OVR_AUDIO_CHECK(Result, "Failed to set listener position and rotation");

		// Translate the input position to OVR coordinates
		FVector OvrPosition = ToOVRVector(Params[InputData.SourceId].EmitterWorldPosition);

		// Set the source position to current audio position
		Result = OVRA_CALL(ovrAudio_SetAudioSourcePos)(Context, InputData.SourceId, OvrPosition.X, OvrPosition.Y, OvrPosition.Z);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source position");

		// Perform the processing
		uint32 Status;
		Result = OVRA_CALL(ovrAudio_SpatializeMonoSourceInterleaved)(Context, InputData.SourceId, &Status, OutputData.AudioBuffer.GetData(), InputData.AudioBuffer->GetData());
		OVR_AUDIO_CHECK(Result, "Failed to spatialize mono source interleaved");
	}
}

bool OculusAudioSpatializationAudioMixer::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OculusAudioSpatializationAudioMixer_Tick);
	if (ContextLock.TryLock())
	{
		if (Context != nullptr)
		{
			ovrResult Result = OVRA_CALL(ovrAudio_UpdateRoomModel)(Context, 1.0f);

			UOculusAudioSettings* settings = GetMutableDefault<UOculusAudioSettings>();
			Result = OVRA_CALL(ovrAudio_SetPropagationQuality)(Context, settings->PropagationQuality);
			if (Result != ovrSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("Bad Propagation Quality setting %d!"), settings->PropagationQuality);
			}
		}

		ContextLock.Unlock();
	}
	
	return true;
}