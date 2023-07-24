// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreAudioOutput.h"

#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"
#include "Sound/AudioSettings.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMediaIOAudioOutput, Log, All);

#define LOCTEXT_NAMESPACE "MediaIOAudioOutput"

FMediaIOAudioOutput::FMediaIOAudioOutput(Audio::FPatchOutputStrongPtr InPatchOutput, const FAudioOptions& InAudioOptions)
    : NumInputChannels(InAudioOptions.InNumInputChannels)
    , NumOutputChannels(InAudioOptions.InNumOutputChannels)
    , TargetFrameRate(InAudioOptions.InTargetFrameRate)
    , MaxSampleLatency(InAudioOptions.InMaxSampleLatency)
    , OutputSampleRate(InAudioOptions.InOutputSampleRate)
	, PatchOutput(MoveTemp(InPatchOutput))
{
	NumSamplesPerFrame = FMath::CeilToInt(NumInputChannels * OutputSampleRate / TargetFrameRate.AsDecimal());
}

int32 FMediaIOAudioOutput::GetAudioBuffer(int32 InNumSamplesToPop, float* OutBuffer) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAudioOutput::GetAudioBuffer);

	if (PatchOutput)
	{
		constexpr bool bUseLatestAudio = false;
		return PatchOutput->MixInAudio(OutBuffer, InNumSamplesToPop, bUseLatestAudio);
	}
	return 0;
}

Audio::FAlignedFloatBuffer FMediaIOAudioOutput::GetFloatBuffer(uint32 NumSamplesToGet) const
{
	// NumSamplesToPop must be a multiple of 4 in order to avoid an assertion in the audio pipeline.
	const int32 NumSamplesToPop = Align(NumSamplesToGet, 4);

	Audio::FAlignedFloatBuffer FloatBuffer;
	FloatBuffer.SetNumZeroed(NumSamplesToPop);

	const int32 NumPopped = GetAudioBuffer(NumSamplesToPop, FloatBuffer.GetData());
	FloatBuffer.SetNum(NumPopped);

	return FloatBuffer;
}

FMediaIOAudioCapture::FMediaIOAudioCapture(const FAudioDeviceHandle& InAudioDeviceHandle)
{
	RegisterAudioDevice(InAudioDeviceHandle);
}

FMediaIOAudioCapture::~FMediaIOAudioCapture()
{
	UnregisterAudioDevice();
}

void FMediaIOAudioCapture::OnNewSubmixBuffer(const USoundSubmix* InOwningSubmix, float* InAudioData, int32 InNumSamples, int32 InNumChannels, const int32 InSampleRate, double InAudioClock)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMediaIOAudioCapture::OnNewSubmixBuffer);

	check(InOwningSubmix);
	if (InOwningSubmix->GetFName() == PrimarySubmixName)
	{
		if (ensureMsgf(NumChannels == InNumChannels, TEXT("Expected %d channels from submix buffer but got %d instead."), NumChannels, InNumChannels))
		{
			AudioCapturedDelegate.ExecuteIfBound(InAudioData, InNumSamples);
			
			int32 NumPushed = AudioSplitter.PushAudio(InAudioData, InNumSamples);
			if (InNumSamples != NumPushed && NumPushed != -1)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FMediaIOAudioCapture::OnNewSubmixBuffer::BufferOverrun);
				UE_LOG(LogMediaIOAudioOutput, Verbose, TEXT("Pushed samples mismatch, Incoming samples: %d, Pushed samples: %d"), InNumSamples, NumPushed);
			}
		}
	}
}

TSharedPtr<FMediaIOAudioOutput> FMediaIOAudioCapture::CreateAudioOutput(int32 InNumOutputChannels, FFrameRate InTargetFrameRate, uint32 InMaxSampleLatency, uint32 InOutputSampleRate)
{
	if (NumChannels > InNumOutputChannels)
	{
		UE_LOG(LogMediaIOAudioOutput, Error, TEXT("Audio capture initialization error, please change the audio output channel count to a number greater or equal to %d."), NumChannels);

#if WITH_EDITOR
		const FText WarningText = FText::Format(LOCTEXT("AudioCaptureError", "Audio capture initialization error, please change the audio output channel count to a number greather or equal to %d."), NumChannels);
		FNotificationInfo WarningNotification(WarningText);
		WarningNotification.bFireAndForget = true;
		WarningNotification.ExpireDuration = 6.0f;
		WarningNotification.bUseThrobber = false;

		// For adding notifications.
		FSlateNotificationManager::Get().AddNotification(WarningNotification);
#endif
		return nullptr;
	}
	
	if (ensureMsgf(InOutputSampleRate == SampleRate, TEXT("The engine's sample rate is different from the output sample rate and resampling is not yet supported in Media Captutre.")))
	{
		constexpr float Gain = 1.0f;
		check(InNumOutputChannels > 0);
		
		Audio::FPatchOutputStrongPtr PatchOutput = AudioSplitter.AddNewPatch(InMaxSampleLatency, Gain);
		FMediaIOAudioOutput::FAudioOptions Options;

		Options.InNumInputChannels = NumChannels;
		Options.InNumOutputChannels = InNumOutputChannels;
		Options.InTargetFrameRate = InTargetFrameRate;
		Options.InMaxSampleLatency = InMaxSampleLatency;
		Options.InOutputSampleRate = InOutputSampleRate;

		return MakeShared<FMediaIOAudioOutput>(MoveTemp(PatchOutput), Options);
	}

	return nullptr;
}

void FMediaIOAudioCapture::RegisterBufferListener(FAudioDevice* AudioDevice)
{
	if (AudioDevice && AudioDevice->IsAudioMixerEnabled())
	{
		const Audio::FMixerDevice* MixerDevice = static_cast<Audio::FMixerDevice*>(AudioDevice);
		NumChannels = MixerDevice->GetDeviceOutputChannels();
		SampleRate = MixerDevice->GetSampleRate();
		PrimarySubmixName = *GetDefault<UAudioSettings>()->MasterSubmix.GetAssetName();
		AudioDevice->RegisterSubmixBufferListener(this);
	}
}

void FMediaIOAudioCapture::UnregisterBufferListener(FAudioDevice* AudioDevice)
{
	if (AudioDevice)
	{
		AudioDevice->UnregisterSubmixBufferListener(this);
	}
}

void FMediaIOAudioCapture::RegisterAudioDevice(const FAudioDeviceHandle& InAudioDeviceHandle)
{
	// Can only be registered to one device.
	UnregisterAudioDevice();

	if (InAudioDeviceHandle.IsValid())
	{
		RegisterBufferListener(InAudioDeviceHandle.GetAudioDevice());
		RegisteredDeviceId = InAudioDeviceHandle.GetDeviceID();
	}
}

void FMediaIOAudioCapture::UnregisterAudioDevice()
{
	if (RegisteredDeviceId != INDEX_NONE)
	{
		if (FAudioDevice* RegisteredDevice = GEngine->GetAudioDeviceManager()->GetAudioDeviceRaw(RegisteredDeviceId))
		{
			UnregisterBufferListener(RegisteredDevice);
		}
		RegisteredDeviceId = INDEX_NONE;
	}
}

FMainMediaIOAudioCapture::FMainMediaIOAudioCapture()
	: FMediaIOAudioCapture(FAudioDeviceHandle())
{
	RegisterMainAudioDevice();
#if WITH_EDITOR
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FMainMediaIOAudioCapture::OnPIEStarted);
	FEditorDelegates::PrePIEEnded.AddRaw(this, &FMainMediaIOAudioCapture::OnPIEEnded);
#endif
}

FMainMediaIOAudioCapture::~FMainMediaIOAudioCapture()
{
#if WITH_EDITOR
	FEditorDelegates::EndPIE.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
#endif

	UnregisterMainAudioDevice();
}

#if WITH_EDITOR
void FMainMediaIOAudioCapture::OnPIEStarted(const bool)
{
	UnregisterMainAudioDevice();
	
	if (GEditor)
	{
		if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
		{
			Audio::FMixerDevice* MixerDevice = static_cast<Audio::FMixerDevice*>(PIEWorldContext->World()->GetAudioDeviceRaw());
			RegisterBufferListener(MixerDevice);
		}
	}
}

void FMainMediaIOAudioCapture::OnPIEEnded(const bool)
{
	if (GEditor)
	{
		if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
		{
			UnregisterBufferListener(PIEWorldContext->World()->GetAudioDeviceRaw());
		}
	}
	
	RegisterMainAudioDevice();
}
#endif

void FMainMediaIOAudioCapture::RegisterMainAudioDevice()
{
	RegisterBufferListener(GEngine->GetMainAudioDeviceRaw());
}

void FMainMediaIOAudioCapture::UnregisterMainAudioDevice()
{
	UnregisterBufferListener(GEngine->GetMainAudioDeviceRaw());
}

#undef LOCTEXT_NAMESPACE //MediaIOAudioOutput