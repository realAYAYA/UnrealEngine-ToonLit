// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAmbisonicSpatializer.h"
#include "ISoundfieldFormat.h"
#include "OculusAudioMixer.h"
#include "DSP/FloatArrayMath.h"
#include "AudioMixerDevice.h"
#include "OculusAudioDllManager.h"
#include "OculusAudioContextManager.h"
#include "SoundFieldRendering.h"
#include "AudioPluginUtilities.h"

namespace OculusAudioUtils
{
	bool IsOculusAudioTheCurrentSpatializationPlugin()
	{
#if WITH_EDITOR
		static FString OculusDisplayName = FString(TEXT("Oculus Audio"));
		return AudioPluginUtilities::GetDesiredPluginName(EAudioPlugin::SPATIALIZATION).Equals(OculusDisplayName);
#else
		// For non editor situations, we can cache whether this is the current plugin or not the first time we check.
		static FString OculusDisplayName = FString(TEXT("Oculus Audio"));
		static bool bCheckedSpatializationPlugin = false;
		static bool bIsOculusCurrentSpatiatizationPlugin = false;

		if (!bCheckedSpatializationPlugin)
		{
			bIsOculusCurrentSpatiatizationPlugin = AudioPluginUtilities::GetDesiredPluginName(EAudioPlugin::SPATIALIZATION).Equals(OculusDisplayName);
			bCheckedSpatializationPlugin = true;
		}

		return bIsOculusCurrentSpatiatizationPlugin;
#endif
	}
}

// the Oculus Audio renderer renders all inputs immediately to an interleaved stereo output.
class FOculusSoundfieldBuffer : public ISoundfieldAudioPacket
{
public:
	// Interleaved binaural audio buffer.
	Audio::FAlignedFloatBuffer AudioBuffer;
	int32 NumChannels;

	FOculusSoundfieldBuffer()
		: NumChannels(0)
	{}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << AudioBuffer;
		Ar << NumChannels;
	}


	virtual TUniquePtr<ISoundfieldAudioPacket> Duplicate() const override
	{
		return TUniquePtr<ISoundfieldAudioPacket>(new FOculusSoundfieldBuffer(*this));
	}


	virtual void Reset() override
	{
		AudioBuffer.Reset();
		NumChannels = 0;
	}

};

class FOculusEncoder : public ISoundfieldEncoderStream
{
private:
	ovrAudioContext Context;
	TArray<int32> SourceIds;

	// temp buffers use to binauralize each source.
	Audio::FAlignedFloatBuffer ScratchMonoBuffer;
	Audio::FAlignedFloatBuffer ScratchOutputBuffer;

	static int32 SourceIdCounter;
	static FCriticalSection SourceIdCounterCritSection;

public:

	FOculusEncoder(ovrAudioContext InContext, int32 InNumChannels, int32 MaxNumSources)
		: Context(InContext)
	{
		check(Context != nullptr);
		{
			// Assign source Ids for each channel here.
			FScopeLock ScopeLock(&SourceIdCounterCritSection);
			for (int32 Index = 0; Index < InNumChannels; Index++)
			{

				SourceIds.Add(SourceIdCounter);
				SourceIdCounter = (SourceIdCounter + 1) % MaxNumSources;
			}
		}

		// Set default settings for each source channel here.
		for (int32& SourceId : SourceIds)
		{
			uint32 Flags = 0;
			Flags |= ovrAudioSourceFlag_ReflectionsDisabled;

			ovrResult Result = OVRA_CALL(ovrAudio_SetAudioSourceFlags)(Context, SourceId, Flags);
			check(Result == ovrSuccess);

			ovrAudioSourceAttenuationMode mode = ovrAudioSourceAttenuationMode_None;
			Result = OVRA_CALL(ovrAudio_SetAudioSourceAttenuationMode)(Context, SourceId, mode, 1.0f);
			check(Result == ovrSuccess);

			Result = OVRA_CALL(ovrAudio_SetAudioReverbSendLevel)(Context, SourceId, 0.0f);
			check(Result == ovrSuccess);
		}
	}

	virtual void Encode(const FSoundfieldEncoderInputData& InputData, ISoundfieldAudioPacket& OutputData) override
	{
		OutputData.Reset();
		EncodeAndMixIn(InputData, OutputData);
	}


	// Binaurally spatialize each independent speaker position. If the spatialization plugin for the current platform was set to Oculus Audio
	// when this encoder was created this will no-op.
	virtual void EncodeAndMixIn(const FSoundfieldEncoderInputData& InputData, ISoundfieldAudioPacket& OutputData) override
	{
		FOculusSoundfieldBuffer& OutputBuffer = DowncastSoundfieldRef<FOculusSoundfieldBuffer>(OutputData);

		check(InputData.NumChannels == SourceIds.Num());
		const int32 NumFrames = InputData.AudioBuffer.Num() / InputData.NumChannels;

		OutputBuffer.AudioBuffer.Reset();
		OutputBuffer.NumChannels = 2;
		OutputBuffer.AudioBuffer.AddZeroed(NumFrames * OutputBuffer.NumChannels);

		FQuat ListenerOrientation = InputData.PositionalData.Rotation.Inverse();

		// Translate the input position to OVR coordinates
		FVector ListenerPosition = OculusAudioSpatializationAudioMixer::ToOVRVector(FVector::ZeroVector);
		FVector ListenerForward = OculusAudioSpatializationAudioMixer::ToOVRVector(ListenerOrientation.GetForwardVector());
		FVector OvrListenerUp = OculusAudioSpatializationAudioMixer::ToOVRVector(ListenerOrientation.GetUpVector());

		ovrResult Result = OVRA_CALL(ovrAudio_SetListenerVectors)(Context,
			ListenerPosition.X, ListenerPosition.Y, ListenerPosition.Z,
			ListenerForward.X, ListenerForward.Y, ListenerForward.Z,
			OvrListenerUp.X, OvrListenerUp.Y, OvrListenerUp.Z);
		OVR_AUDIO_CHECK(Result, "Failed to set listener position and rotation");

		check(InputData.PositionalData.ChannelPositions);
		const TArray<Audio::FChannelPositionInfo>& ChannelPositions = *(InputData.PositionalData.ChannelPositions);

		for (int32 ChannelIndex = 0; ChannelIndex < SourceIds.Num(); ChannelIndex++)
		{
			int32& SourceId = SourceIds[ChannelIndex];
			const Audio::FChannelPositionInfo& ChannelPosition = ChannelPositions[ChannelIndex];
			

			// Translate the input position to OVR coordinates
			FVector SourcePosition = OculusAudioSpatializationAudioMixer::ToOVRVector(ChannelPosition);

			// Set the source position to current audio position
			Result = OVRA_CALL(ovrAudio_SetAudioSourcePos)(Context, SourceId, SourcePosition.X, SourcePosition.Y, SourcePosition.Z);
			OVR_AUDIO_CHECK(Result, "Failed to set audio source position");

			// Deinterleave the audio into the mono temp buffer.
			ScratchMonoBuffer.Reset();
			ScratchMonoBuffer.AddUninitialized(NumFrames);
			for(int32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex++)
			{
				ScratchMonoBuffer[FrameIndex] = InputData.AudioBuffer[FrameIndex * InputData.NumChannels + ChannelIndex];
			}

			ScratchOutputBuffer.Reset();
			ScratchOutputBuffer.AddUninitialized(NumFrames * 2);

			uint32 Status = 0;
			Result = OVRA_CALL(ovrAudio_SpatializeMonoSourceInterleaved)(Context, SourceId, &Status, ScratchOutputBuffer.GetData(), ScratchMonoBuffer.GetData());
			OVR_AUDIO_CHECK(Result, "Failed to spatialize mono source interleaved");

			// sum the interleaved output into the output bed.
			Audio::ArrayMixIn(ScratchOutputBuffer, OutputBuffer.AudioBuffer);
		}
	}
};

int32 FOculusEncoder::SourceIdCounter = 0;
FCriticalSection FOculusEncoder::SourceIdCounterCritSection;

// Because Oculus spatializes every directly to an interleaved stereo buffer,
// we simply pass it forward here.
class FOculusAudioDecoder : public ISoundfieldDecoderStream
{
private:
	const bool bShouldUseSubmixForOutput;

public:
	FOculusAudioDecoder(const bool bInShouldUseSubmixForOutput)
		: bShouldUseSubmixForOutput(bInShouldUseSubmixForOutput)
	{}

	FOculusAudioDecoder() = delete;

	virtual void Decode(const FSoundfieldDecoderInputData& InputData, FSoundfieldDecoderOutputData& OutputData) override
	{
		OutputData.AudioBuffer.Reset();
		OutputData.AudioBuffer.AddZeroed(InputData.NumFrames * InputData.PositionalData.NumChannels);
		DecodeAndMixIn(InputData, OutputData);
	}

	virtual void DecodeAndMixIn(const FSoundfieldDecoderInputData& InputData, FSoundfieldDecoderOutputData& OutputData) override
	{
		if (bShouldUseSubmixForOutput)
		{
			const FOculusSoundfieldBuffer& InputBuffer = DowncastSoundfieldRef<const FOculusSoundfieldBuffer>(InputData.SoundfieldBuffer);

			if (InputBuffer.AudioBuffer.Num() == 0 || InputBuffer.NumChannels == 0)
			{
				return;
			}
			else if (InputData.PositionalData.NumChannels == InputBuffer.NumChannels)
			{
				// If the number of output channels is the same as the input channels, mix it in directly.
				Audio::ArrayMixIn(InputBuffer.AudioBuffer, OutputData.AudioBuffer);
			}
			else
			{
				// Otherwise, downmix and mix in.
				Audio::FAlignedFloatBuffer OutChannelMap;
				Audio::FMixerDevice::Get2DChannelMap(false, InputBuffer.NumChannels, InputData.PositionalData.NumChannels, false, OutChannelMap);
				Audio::DownmixAndSumIntoBuffer(InputBuffer.NumChannels, InputData.PositionalData.NumChannels, InputBuffer.AudioBuffer, OutputData.AudioBuffer, OutChannelMap.GetData());
			}
		}
	}
};

// Class that takes ambisonics audio and renders it to an FOculusSoundfieldBuffer.
class FOculusAmbisonicsTranscoder : public ISoundfieldTranscodeStream
{
private:
	ovrAudioContext Context;
	ovrAudioAmbisonicStream AmbiStreamObject;
public:
	FOculusAmbisonicsTranscoder(ovrAudioContext InContext, int32 InSampleRate, int32 InBufferLength)
		: Context(InContext)
	{
		ovrAudioAmbisonicFormat StreamFormat = ovrAudioAmbisonicFormat_AmbiX;
		ovrResult OurResult = OVRA_CALL(ovrAudio_CreateAmbisonicStream)(Context, InSampleRate, InBufferLength, StreamFormat, 1, &AmbiStreamObject);
		check(OurResult == 0);
		check(AmbiStreamObject != nullptr);
	}

	virtual void Transcode(const ISoundfieldAudioPacket& InputData, const ISoundfieldEncodingSettingsProxy& InputSettings, ISoundfieldAudioPacket& OutputData, const ISoundfieldEncodingSettingsProxy& OutputSettings) override
	{
		OutputData.Reset();
		TranscodeAndMixIn(InputData, InputSettings, OutputData, OutputSettings);
	}


	virtual void TranscodeAndMixIn(const ISoundfieldAudioPacket& InputData, const ISoundfieldEncodingSettingsProxy& InputSettings, ISoundfieldAudioPacket& PacketToSumTo, const ISoundfieldEncodingSettingsProxy& OutputSettings) override
	{
		const FAmbisonicsSoundfieldBuffer& InputBuffer = DowncastSoundfieldRef<const FAmbisonicsSoundfieldBuffer>(InputData);
		FOculusSoundfieldBuffer& OutputBuffer = DowncastSoundfieldRef<FOculusSoundfieldBuffer>(PacketToSumTo);

		if (!InputBuffer.NumChannels || InputBuffer.AudioBuffer.IsEmpty())
		{
			return;
		}

		const int32 NumFrames = InputBuffer.AudioBuffer.Num() / InputBuffer.NumChannels;
		OutputBuffer.AudioBuffer.SetNumZeroed(NumFrames * 2);
		OutputBuffer.NumChannels = 2;

		//Currently, Oculus only decodes first-order ambisonics to stereo.
		// in the future we can truncate InputBuffer to four channels and 
		// render that.
		// For now we just output silence for higher order ambisonics from the Oculus spatializaers
		if (InputBuffer.NumChannels == 4)
		{
			// Translate the input position to OVR coordinates
			const FQuat& ListenerRotation = InputBuffer.Rotation;
			FVector OvrListenerForward = OculusAudioSpatializationAudioMixer::ToOVRVector(ListenerRotation.GetForwardVector());
			FVector OvrListenerUp = OculusAudioSpatializationAudioMixer::ToOVRVector(ListenerRotation.GetUpVector());

			static auto SetListenerVectors = OVRA_CALL(ovrAudio_SetListenerVectors);
			ovrResult Result = SetListenerVectors(Context,
				0.0f, 0.0f, 0.0f,
				OvrListenerForward.X, OvrListenerForward.Y, OvrListenerForward.Z,
				OvrListenerUp.X, OvrListenerUp.Y, OvrListenerUp.Z);

			ovrResult DecodeResult = OVRA_CALL(ovrAudio_ProcessAmbisonicStreamInterleaved)(Context, AmbiStreamObject, InputBuffer.AudioBuffer.GetData(), OutputBuffer.AudioBuffer.GetData(), NumFrames);
			check(DecodeResult == 0);
		}
	}

};

// Since FOculusSoundfieldBuffer is just an interleaved stereo buffer, we simply mix the buffer here.
class FOculusAudioSoundfieldMixer : public ISoundfieldMixerStream
{
public:
	virtual void MixTogether(const FSoundfieldMixerInputData& InputData, ISoundfieldAudioPacket& PacketToSumTo) override
	{
		const FOculusSoundfieldBuffer& InputBuffer = DowncastSoundfieldRef<const FOculusSoundfieldBuffer>(InputData.InputPacket);
		FOculusSoundfieldBuffer& OutputBuffer = DowncastSoundfieldRef<FOculusSoundfieldBuffer>(PacketToSumTo);

		if (OutputBuffer.AudioBuffer.Num() == 0)
		{
			OutputBuffer.AudioBuffer = InputBuffer.AudioBuffer;
			OutputBuffer.NumChannels = InputBuffer.NumChannels;
		}
		else
		{
			check(InputBuffer.AudioBuffer.Num() == OutputBuffer.AudioBuffer.Num());
			check(InputBuffer.NumChannels == OutputBuffer.NumChannels);


			Audio::ArrayMixIn(InputBuffer.AudioBuffer, OutputBuffer.AudioBuffer, InputData.SendLevel);
		}
	}
};

FOculusAmbisonicsFactory::FOculusAmbisonicsFactory()
{
	ISoundfieldFactory::RegisterSoundfieldFormat(this);
}

FOculusAmbisonicsFactory::~FOculusAmbisonicsFactory()
{
	ISoundfieldFactory::UnregisterSoundfieldFormat(this);
}

bool FOculusAmbisonicsFactory::IsTranscodeRequiredBetweenSettings(const ISoundfieldEncodingSettingsProxy& SourceSettings, const ISoundfieldEncodingSettingsProxy& DestinationSettings)
{
	return false;
}

bool FOculusAmbisonicsFactory::CanTranscodeFromSoundfieldFormat(FName SourceFormat, const ISoundfieldEncodingSettingsProxy& SourceEncodingSettings)
{
	FName AmbisonicsSoundfieldFormatName = GetUnrealAmbisonicsFormatName();
	if (SourceFormat == AmbisonicsSoundfieldFormatName)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool FOculusAmbisonicsFactory::CanTranscodeToSoundfieldFormat(FName DestinationFormat, const ISoundfieldEncodingSettingsProxy& DestinationEncodingSettings)
{
	return false;
}

UClass* FOculusAmbisonicsFactory::GetCustomEncodingSettingsClass() const
{
	return UOculusAudioSoundfieldSettings::StaticClass();
}

const USoundfieldEncodingSettingsBase* FOculusAmbisonicsFactory::GetDefaultEncodingSettings()
{
	return GetDefault<UOculusAudioSoundfieldSettings>();
}

FName FOculusAmbisonicsFactory::GetSoundfieldFormatName()
{
	static FName OculusBinauralFormatName = FName(TEXT("Oculus Binaural"));
	return OculusBinauralFormatName;
}

TUniquePtr<ISoundfieldEncoderStream> FOculusAmbisonicsFactory::CreateEncoderStream(const FAudioPluginInitializationParams& InitInfo, const ISoundfieldEncodingSettingsProxy& InitialSettings)
{
	ovrAudioContext Context = FOculusAudioContextManager::GetContextForAudioDevice(InitInfo.AudioDevicePtr);
	if (!Context)
	{
		Context = FOculusAudioContextManager::CreateContextForAudioDevice(InitInfo.AudioDevicePtr);
		check(Context);
	}

	return TUniquePtr<ISoundfieldEncoderStream>(new FOculusEncoder(Context, InitInfo.NumOutputChannels, InitInfo.AudioDevicePtr->GetMaxChannels()));
}

TUniquePtr<ISoundfieldDecoderStream> FOculusAmbisonicsFactory::CreateDecoderStream(const FAudioPluginInitializationParams& InitInfo, const ISoundfieldEncodingSettingsProxy& InitialSettings)
{
	const bool bShouldOutputOculusAudio = !OculusAudioUtils::IsOculusAudioTheCurrentSpatializationPlugin();

	UE_CLOG(!bShouldOutputOculusAudio, LogAudio, Warning, TEXT("Oculus Audio is the currently selected spatialization plugin. The Oculus soundfield submix's output will be ignored."));

	return TUniquePtr<ISoundfieldDecoderStream>(new FOculusAudioDecoder(bShouldOutputOculusAudio));
}

TUniquePtr<ISoundfieldTranscodeStream> FOculusAmbisonicsFactory::CreateTranscoderStream(const FName SourceFormat, const ISoundfieldEncodingSettingsProxy& InitialSourceSettings, const FName DestinationFormat, const ISoundfieldEncodingSettingsProxy& InitialDestinationSettings, const FAudioPluginInitializationParams& InitInfo)
{
	check(SourceFormat == GetUnrealAmbisonicsFormatName());
	check(DestinationFormat == GetSoundfieldFormatName());

	ovrAudioContext Context = FOculusAudioContextManager::GetContextForAudioDevice(InitInfo.AudioDevicePtr);
	if (!Context)
	{
		Context = FOculusAudioContextManager::CreateContextForAudioDevice(InitInfo.AudioDevicePtr);
		check(Context);
	}

	return TUniquePtr<ISoundfieldTranscodeStream>(new FOculusAmbisonicsTranscoder(Context, InitInfo.SampleRate, InitInfo.BufferLength));
}

TUniquePtr<ISoundfieldMixerStream> FOculusAmbisonicsFactory::CreateMixerStream(const ISoundfieldEncodingSettingsProxy& InitialSettings)
{
	return TUniquePtr<ISoundfieldMixerStream>(new FOculusAudioSoundfieldMixer());
}

TUniquePtr<ISoundfieldAudioPacket> FOculusAmbisonicsFactory::CreateEmptyPacket()
{
	return TUniquePtr<ISoundfieldAudioPacket>(new FOculusSoundfieldBuffer());
}
