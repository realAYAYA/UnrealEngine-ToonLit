// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMixer.h"

#include "AudioDefines.h"
#include "AudioMixerTrace.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/FloatArrayMath.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Event.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeTryLock.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Trace/Trace.h"


// Defines the "Audio" category in the CSV profiler.
// This should only be defined here. Modules who wish to use this category should contain the line
// 		CSV_DECLARE_CATEGORY_MODULE_EXTERN(AUDIOMIXERCORE_API, Audio);
//
CSV_DEFINE_CATEGORY_MODULE(AUDIOMIXERCORE_API, Audio, false);

#if UE_AUDIO_PROFILERTRACE_ENABLED
UE_TRACE_CHANNEL_DEFINE(AudioChannel);
UE_TRACE_CHANNEL_DEFINE(AudioMixerChannel);
#endif // UE_AUDIO_PROFILERTRACE_ENABLED


// Command to enable logging to display accurate audio render times
static int32 LogRenderTimesCVar = 0;
FAutoConsoleVariableRef CVarLogRenderTimes(
	TEXT("au.LogRenderTimes"),
	LogRenderTimesCVar,
	TEXT("Logs Audio Render Times.\n")
	TEXT("0: Not Log, 1: Log"),
	ECVF_Default);

static float MinTimeBetweenUnderrunWarningsMs = 1000.f*10.f;
FAutoConsoleVariableRef CVarMinTimeBetweenUnderrunWarningsMs(
	TEXT("au.MinLogTimeBetweenUnderrunWarnings"),
	MinTimeBetweenUnderrunWarningsMs,
	TEXT("Min time between underrun warnings (globally) in MS\n")
	TEXT("Set the time between each subsequent underrun log warning globaly (defaults to 10secs)"),
	ECVF_Default);

// Command for setting the audio render thread priority.
static int32 SetRenderThreadPriorityCVar = (int32)TPri_Highest;
FAutoConsoleVariableRef CVarSetRenderThreadPriority(
	TEXT("au.RenderThreadPriority"),
	SetRenderThreadPriorityCVar,
	TEXT("Sets audio render thread priority. Defaults to 3.\n")
	TEXT("0: Normal, 1: Above Normal, 2: Below Normal, 3: Highest, 4: Lowest, 5: Slightly Below Normal, 6: Time Critical"),
	ECVF_Default);

static int32 SetRenderThreadAffinityCVar = 0;
FAutoConsoleVariableRef CVarRenderThreadAffinity(
	TEXT("au.RenderThreadAffinity"),
	SetRenderThreadAffinityCVar,
	TEXT("Override audio render thread affinity.\n")
	TEXT("0: Disabled (Default), otherwise overriden thread affinity."),
	ECVF_Default);


static int32 EnableDetailedWindowsDeviceLoggingCVar = 0;
FAutoConsoleVariableRef CVarEnableDetailedWindowsDeviceLogging(
	TEXT("au.EnableDetailedWindowsDeviceLogging"),
	EnableDetailedWindowsDeviceLoggingCVar,
	TEXT("Enables detailed windows device logging.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

static int32 DisableDeviceSwapCVar = 0;
FAutoConsoleVariableRef CVarDisableDeviceSwap(
	TEXT("au.DisableDeviceSwap"),
	DisableDeviceSwapCVar,
	TEXT("Disable device swap handling code for Audio Mixer on Windows.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

static int32 bUseThreadedDeviceSwapCVar = 1;
FAutoConsoleVariableRef CVarUseThreadedDeviceSwap(
	TEXT("au.UseThreadedDeviceSwap"),
	bUseThreadedDeviceSwapCVar,
	TEXT("Lets Device Swap go wide.")
	TEXT("0 off, 1 on"),
	ECVF_Default);

static int32 bUseAudioDeviceInfoCacheCVar = 1;
FAutoConsoleVariableRef CVarUseAudioDeviceInfoCache(
	TEXT("au.UseCachedDeviceInfoCache"),
	bUseAudioDeviceInfoCacheCVar,
	TEXT("Uses a Cache of the DeviceCache instead of asking the OS")
	TEXT("0 off, 1 on"),
	ECVF_Default);
	
static int32 bRecycleThreadsCVar = 1;
FAutoConsoleVariableRef CVarRecycleThreads(
	TEXT("au.RecycleThreads"),
	bRecycleThreadsCVar,
	TEXT("Keeps threads to reuse instead of create/destroying them")
	TEXT("0 off, 1 on"),
	ECVF_Default);

static int32 OverrunTimeoutCVar = 5000;
FAutoConsoleVariableRef CVarOverrunTimeout(
	TEXT("au.OverrunTimeoutMSec"),
	OverrunTimeoutCVar,
	TEXT("Amount of time to wait for the render thread to time out before swapping to the null device. \n"),
	ECVF_Default);

static int32 UnderrunTimeoutCVar = 5;
FAutoConsoleVariableRef CVarUnderrunTimeout(
	TEXT("au.UnderrunTimeoutMSec"),
	UnderrunTimeoutCVar,
	TEXT("Amount of time to wait for the render thread to generate the next buffer before submitting an underrun buffer. \n"),
	ECVF_Default);

static int32 FadeoutTimeoutCVar = 2000;
FAutoConsoleVariableRef CVarFadeoutTimeout(
	TEXT("au.FadeOutTimeoutMSec"),
	FadeoutTimeoutCVar,
	TEXT("Amount of time to wait for the FadeOut Event to fire. \n"),
	ECVF_Default);

static float LinearGainScalarForFinalOututCVar = 1.0f;
FAutoConsoleVariableRef LinearGainScalarForFinalOutut(
	TEXT("au.LinearGainScalarForFinalOutut"),
	LinearGainScalarForFinalOututCVar,
	TEXT("Linear gain scalar applied to the final float buffer to allow for hotfixable mitigation of clipping \n")
	TEXT("Default is 1.0f \n"),
	ECVF_Default);

static int32 ExtraAudioMixerDeviceLoggingCVar = 0;
FAutoConsoleVariableRef ExtraAudioMixerDeviceLogging(
	TEXT("au.ExtraAudioMixerDeviceLogging"),
	ExtraAudioMixerDeviceLoggingCVar,
	TEXT("Enables extra logging for audio mixer device running \n")
	TEXT("0: no logging, 1: logging every 500 callbacks \n"),
	ECVF_Default);

// Stat definitions for profiling audio mixer 
DEFINE_STAT(STAT_AudioMixerRenderAudio);

namespace Audio
{
	int32 sRenderInstanceIds = 0;

	FThreadSafeCounter AudioMixerTaskCounter;

	FAudioRenderTimeAnalysis::FAudioRenderTimeAnalysis()
		: AvgRenderTime(0.0)
		, MaxRenderTime(0.0)
		, TotalRenderTime(0.0)
		, StartTime(0.0)
		, RenderTimeCount(0)
		, RenderInstanceId(sRenderInstanceIds++)
	{}

	void FAudioRenderTimeAnalysis::Start()
	{
		StartTime = FPlatformTime::Cycles();
	}

	void FAudioRenderTimeAnalysis::End()
	{
		uint32 DeltaCycles = FPlatformTime::Cycles() - StartTime;
		double DeltaTime = DeltaCycles * FPlatformTime::GetSecondsPerCycle();

		TotalRenderTime += DeltaTime;
		RenderTimeSinceLastLog += DeltaTime;
		++RenderTimeCount;
		AvgRenderTime = TotalRenderTime / RenderTimeCount;
		
		if (DeltaTime > MaxRenderTime)
		{
			MaxRenderTime = DeltaTime;
		}
		
		if (DeltaTime > MaxSinceTick)
		{
			MaxSinceTick = DeltaTime;
		}

		if (LogRenderTimesCVar == 1)
		{
			if (RenderTimeCount % 32 == 0)
			{
				RenderTimeSinceLastLog /= 32.0f;
				UE_LOG(LogAudioMixerDebug, Display, TEXT("Render Time [id:%d] - Max: %.2f ms, MaxDelta: %.2f ms, Delta Avg: %.2f ms, Global Avg: %.2f ms"), 
					RenderInstanceId, 
					(float)MaxRenderTime * 1000.0f, 
					(float)MaxSinceTick * 1000.0f,
					RenderTimeSinceLastLog * 1000.0f, 
					(float)AvgRenderTime * 1000.0f);

				RenderTimeSinceLastLog = 0.0f;
				MaxSinceTick = 0.0f;
			}
		}
	}


	void FOutputBuffer::Init(IAudioMixer* InAudioMixer, const int32 InNumSamples, const int32 InNumBuffers, const EAudioMixerStreamDataFormat::Type InDataFormat)
	{
		SCOPED_NAMED_EVENT(FOutputBuffer_Init, FColor::Blue);

		RenderBuffer.Reset();
		RenderBuffer.AddUninitialized(InNumSamples);

		DataFormat = InDataFormat;

		check(InAudioMixer != nullptr);
		AudioMixer = InAudioMixer;

		CircularBuffer.SetCapacity(InNumSamples * InNumBuffers * GetSizeForDataFormat(DataFormat));

		PopBuffer.Reset();
		PopBuffer.AddUninitialized(InNumSamples * GetSizeForDataFormat(DataFormat));

		if (DataFormat != EAudioMixerStreamDataFormat::Float)
		{
			FormattedBuffer.SetNumZeroed(InNumSamples * GetSizeForDataFormat(DataFormat));
		}
	}

	bool FOutputBuffer::MixNextBuffer()
 	{
		// If the circular queue is already full, exit.
		if (CircularBuffer.Remainder() < static_cast<uint32>(RenderBuffer.Num()))
		{
			return false;
		}

		CSV_SCOPED_TIMING_STAT(Audio, RenderAudio);
		SCOPE_CYCLE_COUNTER(STAT_AudioMixerRenderAudio);

		// Zero the buffer
		FPlatformMemory::Memzero(RenderBuffer.GetData(), RenderBuffer.Num() * sizeof(float));
		if (AudioMixer != nullptr)
		{
			AudioMixer->OnProcessAudioStream(RenderBuffer);
		}

		switch (DataFormat)
		{
		case EAudioMixerStreamDataFormat::Float:
		{
			if (!FMath::IsNearlyEqual(LinearGainScalarForFinalOututCVar, 1.0f))
			{
				ArrayMultiplyByConstantInPlace(RenderBuffer, LinearGainScalarForFinalOututCVar);
			}
			ArrayRangeClamp(RenderBuffer, -1.0f, 1.0f);

			// No conversion is needed, so we push the RenderBuffer directly to the circular queue.
			CircularBuffer.Push(reinterpret_cast<const uint8*>(RenderBuffer.GetData()), RenderBuffer.Num() * sizeof(float));
		}
		break;

		case EAudioMixerStreamDataFormat::Int16:
		{
			int16* BufferInt16 = (int16*)FormattedBuffer.GetData();
			const int32 NumSamples = RenderBuffer.Num();
			check(FormattedBuffer.Num() / GetSizeForDataFormat(DataFormat) == RenderBuffer.Num());			

			const float ConversionScalar = LinearGainScalarForFinalOututCVar * 32767.0f;
			ArrayMultiplyByConstantInPlace(RenderBuffer, ConversionScalar);
			ArrayRangeClamp(RenderBuffer, -32767.0f, 32767.0f);

			for (int32 i = 0; i < NumSamples; ++i)
			{
				BufferInt16[i] = (int16)RenderBuffer[i];
			}

			CircularBuffer.Push(reinterpret_cast<const uint8*>(FormattedBuffer.GetData()), FormattedBuffer.Num());
		}
		break;

		default:
			// Not implemented/supported
			check(false);
			break;
		}

		static const int32 HeartBeatRate = 500;
		if ((ExtraAudioMixerDeviceLoggingCVar > 0) && (++CallCounterMixNextBuffer > HeartBeatRate))
		{
			UE_LOG(LogAudioMixer, Display, TEXT("FOutputBuffer::MixNextBuffer() called %i times"), HeartBeatRate);
			CallCounterMixNextBuffer = 0;
		}

		return true;
 	}
 
	TArrayView<const uint8> FOutputBuffer::PopBufferData(int32& OutNumBytesPopped) const
	{
		FMemory::Memzero(reinterpret_cast<uint8*>(PopBuffer.GetData()), PopBuffer.Num());
		OutNumBytesPopped = CircularBuffer.Pop(PopBuffer.GetData(), PopBuffer.Num());

		return TArrayView<const uint8>(PopBuffer);
	}

	int32 FOutputBuffer::GetNumSamples() const
	{
		return RenderBuffer.Num();
	}

	size_t FOutputBuffer::GetSizeForDataFormat(EAudioMixerStreamDataFormat::Type InDataFormat)
	{
		switch (InDataFormat)
		{
		case EAudioMixerStreamDataFormat::Float:
			return sizeof(float);

		case EAudioMixerStreamDataFormat::Int16:
			return sizeof(int16);

		default:
			checkNoEntry();
			return 0;
		}
	}

	/**
	 * IAudioMixerPlatformInterface
	 */

	// Static linkage.
	FThreadSafeCounter IAudioMixerPlatformInterface::NextInstanceID;

	IAudioMixerPlatformInterface::IAudioMixerPlatformInterface()
		: bWarnedBufferUnderrun(false)
		, AudioRenderEvent(nullptr)
		, bIsInDeviceSwap(false)
		, AudioFadeEvent(nullptr)
		, NumOutputBuffers(0)
		, FadeVolume(0.0f)
		, LastError(TEXT("None"))
		, bPerformingFade(true)
		, bFadedOut(false)
		, bIsDeviceInitialized(false)
		, bMoveAudioStreamToNewAudioDevice(false)
		, bIsUsingNullDevice(false)
		, bIsGeneratingAudio(false)
		, InstanceID(NextInstanceID.Increment())
		, NullDeviceCallback(nullptr)
	{
		FadeParam.SetValue(0.0f);
	}

	IAudioMixerPlatformInterface::~IAudioMixerPlatformInterface()
	{
		check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Closed);
	}

	void IAudioMixerPlatformInterface::FadeIn()
	{
		if (IsNonRealtime())
		{
			FadeParam.SetValue(1.0f);
		}

		bPerformingFade = true;
		bFadedOut = false;
		FadeVolume = 1.0f;
	}

	void IAudioMixerPlatformInterface::FadeOut()
	{
		// Non Realtime isn't ticked when fade out is called, and the user can't hear
		// the output anyways so there's no need to make it pleasant for their ears.
		if (!FPlatformProcess::SupportsMultithreading() || IsNonRealtime())
		{
			bFadedOut = true;
			FadeVolume = 0.f;
			return;
		}

		if (bFadedOut || FadeVolume == 0.0f)
		{
			return;
		}

		bPerformingFade = true;
		if (AudioFadeEvent != nullptr)
		{						
			if (!AudioFadeEvent->Wait(FadeoutTimeoutCVar))
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("FadeOutEvent timed out"));
			}
		}

		FadeVolume = 0.0f;
	}

	void IAudioMixerPlatformInterface::PostInitializeHardware()
	{
		bIsDeviceInitialized = true;
	}

	int32 IAudioMixerPlatformInterface::GetIndexForDevice(const FString& InDeviceName)
	{
		uint32 TotalNumDevices = 0;

		if (!GetNumOutputDevices(TotalNumDevices))
		{
			return INDEX_NONE;
		}

		// Iterate through every device and see if
		for (uint32 DeviceIndex = 0; DeviceIndex < TotalNumDevices; DeviceIndex++)
		{
			FAudioPlatformDeviceInfo DeviceInfo;
			if (GetOutputDeviceInfo(DeviceIndex, DeviceInfo))
			{
				// check if the device name matches the input device name:
				if (DeviceInfo.Name.Contains(InDeviceName))
				{
					return DeviceIndex;
				}
			}
		}

		// If we've made it here, we weren't able to find a matching device.
		return INDEX_NONE;
	}

	template<typename BufferType>
	void IAudioMixerPlatformInterface::ApplyAttenuationInternal(TArrayView<BufferType>& InOutBuffer)
	{
		static const int32 HeartBeatRate = 500;
		const bool bLog = (ExtraAudioMixerDeviceLoggingCVar > 0) && (++CallCounterApplyAttenuationInternal > HeartBeatRate);
		if (bLog)
		{
			UE_LOG(LogAudioMixer, Display, TEXT("IAudioMixerPlatformInterface::ApplyAttenuationInternal() called %i times"), HeartBeatRate);
			CallCounterApplyAttenuationInternal = 0;
		}

		// Perform fade in and fade out global attenuation to avoid clicks/pops on startup/shutdown
		if (bPerformingFade)
		{
			FadeParam.SetValue(FadeVolume, InOutBuffer.Num());

			for (int32 i = 0; i < InOutBuffer.Num(); ++i)
			{
				InOutBuffer[i] = (BufferType)(InOutBuffer[i] * FadeParam.Update());
			}

			bFadedOut = (FadeVolume == 0.0f);
			bPerformingFade = false;
			AudioFadeEvent->Trigger();

			if (bLog)
			{
				UE_LOG(LogAudioMixer, Display, TEXT("IAudioMixerPlatformInterface::ApplyAttenuationInternal() Faded from %f to %f"), FadeVolume, FadeParam.GetValue());
			}
		}
		else if (bFadedOut)
		{
			// If we're faded out, then just zero the data.
			FPlatformMemory::Memzero((void*)InOutBuffer.GetData(), sizeof(BufferType)* InOutBuffer.Num());

			if (bLog)
			{
				UE_LOG(LogAudioMixer, Display, TEXT("IAudioMixerPlatformInterface::ApplyAttenuationInternal() Zero'd out buffer"));
			}
		}

		FadeParam.Reset();
	}

	void IAudioMixerPlatformInterface::StartRunningNullDevice()
	{
		UE_LOG(LogAudioMixer, Verbose, TEXT("StartRunningNullDevice() called, InstanceID=%d"), InstanceID);
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_StartRunningNullDevice, FColor::Blue);
		
		auto ThrowAwayBuffer = [this]() { this->ReadNextBuffer(); };
		float SafeSampleRate = OpenStreamParams.SampleRate > 0.f ? OpenStreamParams.SampleRate : 48000.f;
		float BufferDuration = ((float)OpenStreamParams.NumFrames) / SafeSampleRate;

		if (AudioRenderEvent)
		{
			AudioRenderEvent->Trigger();
		}

		if (!NullDeviceCallback.IsValid())
		{
			// Create the thread and tell it not to pause.
			CreateNullDeviceThread(ThrowAwayBuffer, BufferDuration, false);
			check(NullDeviceCallback.IsValid());
		}
		else
		{
			// Reuse existing thread if we have one.
			NullDeviceCallback->Resume(ThrowAwayBuffer, BufferDuration);
		}

		bIsUsingNullDevice = true;
	}

	void IAudioMixerPlatformInterface::StopRunningNullDevice()
	{		
		UE_LOG(LogAudioMixer, Verbose, TEXT("StopRunningNullDevice() called, InstanceID=%d"), InstanceID);
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_StopRunningNullDevice, FColor::Blue);

		if (NullDeviceCallback.IsValid())
		{
			if(IAudioMixer::ShouldRecycleThreads())
			{
				NullDeviceCallback->Pause();
			}
			else
			{
				NullDeviceCallback.Reset();
			}
			bIsUsingNullDevice = false;
		}
	}

	void IAudioMixerPlatformInterface::CreateNullDeviceThread(const TFunction<void()> InCallback, float InBufferDuration, bool bShouldPauseOnStart)
	{
		NullDeviceCallback.Reset(new FMixerNullCallback(InBufferDuration, InCallback, TPri_TimeCritical, bShouldPauseOnStart));
	}

	void IAudioMixerPlatformInterface::ApplyPrimaryAttenuation(TArrayView<const uint8>& OutPoppedAudio)
	{
		EAudioMixerStreamDataFormat::Type Format = OutputBuffer.GetFormat();

		if (Format == EAudioMixerStreamDataFormat::Float)
		{
			TArrayView<float> OutFloatBuffer = TArrayView<float>(const_cast<float*>(reinterpret_cast<const float*>(OutPoppedAudio.GetData())), OutPoppedAudio.Num() / sizeof(float));
			ApplyAttenuationInternal(OutFloatBuffer);
		}
		else if (Format == EAudioMixerStreamDataFormat::Int16)
		{
			TArrayView<int16> OutIntBuffer = TArrayView<int16>(const_cast<int16*>(reinterpret_cast<const int16*>(OutPoppedAudio.GetData())), OutPoppedAudio.Num() / sizeof(int16));
			ApplyAttenuationInternal(OutIntBuffer);
		}
		else
		{
			checkNoEntry();
		}
	}

	void IAudioMixerPlatformInterface::ReadNextBuffer()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// If we are flushing buffers for our output voice and this is being called on the audio thread directly,
		// early exit.
		if (bIsInDeviceSwap)
		{
			return;
		}

		// If we are currently swapping devices and OnBufferEnd is being triggered in an XAudio2Thread,
		// early exit.
		if (!DeviceSwapCriticalSection.TryLock())
		{
			return;
		}

		// Don't read any more audio if we're not running or changing device
		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Running)
		{
			DeviceSwapCriticalSection.Unlock();
			return;
		}
		
		static int32 UnderrunCount = 0;
		static int32 CurrentUnderrunCount = 0;
		static uint64 TimeLastWarningCycles = 0;

		int32 NumSamplesPopped = 0;
		TArrayView<const uint8> PoppedAudio = OutputBuffer.PopBufferData(NumSamplesPopped);

		bool bDidOutputUnderrun = NumSamplesPopped != PoppedAudio.Num();
		
		if (bDidOutputUnderrun)
		{
			UnderrunCount++;
			CurrentUnderrunCount++;
			
			if (!bWarnedBufferUnderrun)
			{
				float ElapsedTimeInMs = static_cast<float>(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - TimeLastWarningCycles));
				if( ElapsedTimeInMs > MinTimeBetweenUnderrunWarningsMs )
				{
					// Underrun/Starvation:
					// Things to try: Increase # output buffers, ensure audio-render thread has time to run (affinity and priority), debug your mix and reduce # sounds playing.

					UE_LOG(LogAudioMixer, Display, TEXT("Audio Buffer Underrun (starvation) detected. InstanceID=%d"), InstanceID);
					bWarnedBufferUnderrun = true;
					TimeLastWarningCycles = FPlatformTime::Cycles64();
				}
			}
		}
		else
		{
			// As soon as a valid buffer goes through, allow more warning
			if (bWarnedBufferUnderrun)
			{
				UE_LOG(LogAudioMixerDebug, Log, TEXT("Audio had %d underruns [Total: %d], InstanceID=%d"), CurrentUnderrunCount, UnderrunCount, InstanceID);
			}

			CurrentUnderrunCount = 0;
			bWarnedBufferUnderrun = false;
		}

		ApplyPrimaryAttenuation(PoppedAudio);
		SubmitBuffer(PoppedAudio.GetData());

		DeviceSwapCriticalSection.Unlock();

		// Kick off rendering of the next set of buffers
		if (AudioRenderEvent)
		{
			AudioRenderEvent->Trigger();
		}
	}

	void IAudioMixerPlatformInterface::BeginGeneratingAudio()
	{
		SCOPED_NAMED_EVENT(IAudioMixerPlatformInterface_BeginGeneratingAudio, FColor::Blue);
		
		checkf(!bIsGeneratingAudio, TEXT("BeginGeneratingAudio() is being run with StreamState = %i and bIsGeneratingAudio = %i"), AudioStreamInfo.StreamState, !!bIsGeneratingAudio);

		bIsGeneratingAudio = true;

		// Setup the output buffers
		const int32 NumOutputFrames = OpenStreamParams.NumFrames;
		const int32 NumOutputChannels = AudioStreamInfo.DeviceInfo.NumChannels;
		const int32 NumOutputSamples = NumOutputFrames * NumOutputChannels;

		// Set the number of buffers to be one more than the number to queue.
		NumOutputBuffers = FMath::Max(OpenStreamParams.NumBuffers, 2);
		UE_LOG(LogAudioMixer, Display, TEXT("Output buffers initialized: Frames=%i, Channels=%i, Samples=%i, InstanceID=%d"), NumOutputFrames, NumOutputChannels, NumOutputSamples, InstanceID);


		OutputBuffer.Init(AudioStreamInfo.AudioMixer, NumOutputSamples, NumOutputBuffers, AudioStreamInfo.DeviceInfo.Format);

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;

		check(AudioRenderEvent == nullptr);
		AudioRenderEvent = FPlatformProcess::GetSynchEventFromPool();
		check(AudioRenderEvent != nullptr);

		check(AudioFadeEvent == nullptr);
		AudioFadeEvent = FPlatformProcess::GetSynchEventFromPool();
		check(AudioFadeEvent != nullptr);

		check(!AudioRenderThread.IsValid());
		uint64 RenderThreadAffinityCVar = SetRenderThreadAffinityCVar > 0 ? uint64(SetRenderThreadAffinityCVar) : FPlatformAffinity::GetAudioRenderThreadMask();
		AudioRenderThread.Reset(FRunnableThread::Create(this, *FString::Printf(TEXT("AudioMixerRenderThread(%d)"), AudioMixerTaskCounter.Increment()), 0, (EThreadPriority)SetRenderThreadPriorityCVar, RenderThreadAffinityCVar));
		check(AudioRenderThread.IsValid());
	}

	void IAudioMixerPlatformInterface::StopGeneratingAudio()
	{		
		SCOPED_NAMED_EVENT(IAudioMixerPlatformInterface_StopGeneratingAudio, FColor::Blue);

		// Stop the FRunnable thread

		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped)
		{
			AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopping;
		}

		if (AudioRenderEvent != nullptr)
		{
			// Make sure the thread wakes up
			AudioRenderEvent->Trigger();
		}

		if (AudioRenderThread.IsValid())
		{
			{
				SCOPED_NAMED_EVENT(IAudioMixerPlatformInterface_StopGeneratingAudio_KillRenderThread, FColor::Blue);
				AudioRenderThread->Kill();
			}

			// WaitForCompletion will complete right away when single threaded, and AudioStreamInfo.StreamState will never be set to stopped
			if (FPlatformProcess::SupportsMultithreading())
			{
				check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
			}
			else
			{
				AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopped;
			}

			AudioRenderThread.Reset();
		}

		if (AudioRenderEvent != nullptr)
		{
			FPlatformProcess::ReturnSynchEventToPool(AudioRenderEvent);
			AudioRenderEvent = nullptr;
		}

		if (AudioFadeEvent != nullptr)
		{
			FPlatformProcess::ReturnSynchEventToPool(AudioFadeEvent);
			AudioFadeEvent = nullptr;
		}

		bIsGeneratingAudio = false;
	}

	void IAudioMixerPlatformInterface::Tick()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// In single-threaded mode, we simply render buffers until we run out of space
		// The single-thread audio backend will consume these rendered buffers when they need to
		if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running && bIsDeviceInitialized)
		{
			// Render mixed buffers till our queued buffers are filled up
			while (OutputBuffer.MixNextBuffer())
			{
			}
		}
	}

	uint32 IAudioMixerPlatformInterface::MainAudioDeviceRun()
	{
		return RunInternal();
	}

	uint32 IAudioMixerPlatformInterface::RunInternal()
	{
		UE_LOG(LogAudioMixer, Display, TEXT("Starting AudioMixerPlatformInterface::RunInternal(), InstanceID=%d"), InstanceID);

		// Lets prime and submit the first buffer (which is going to be the buffer underrun buffer)
		int32 NumSamplesPopped;
		TArrayView<const uint8> AudioToSubmit = OutputBuffer.PopBufferData(NumSamplesPopped);

		SubmitBuffer(AudioToSubmit.GetData());

		OutputBuffer.MixNextBuffer();

		while (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopping)
		{
			// Render mixed buffers till our queued buffers are filled up
			while (bIsDeviceInitialized && OutputBuffer.MixNextBuffer())
			{
			}

			// Bounds check the timeout for our audio render event.
			OverrunTimeoutCVar = FMath::Clamp(OverrunTimeoutCVar, 500, 5000);

			// If we're debugging, make the timeout the maximum to avoid needless swaps.
			OverrunTimeoutCVar = FPlatformMisc::IsDebuggerPresent() ? TNumericLimits<uint32>::Max() : OverrunTimeoutCVar;

			// Now wait for a buffer to be consumed, which will bump up the read index.
			const double WaitStartTime = FPlatformTime::Seconds();
			if (AudioRenderEvent && !AudioRenderEvent->Wait(static_cast<uint32>(OverrunTimeoutCVar)))
			{
				// if we reached this block, we timed out, and should attempt to
				// bail on our current device.
				RequestDeviceSwap(TEXT(""), /* force */true, TEXT("AudioMixerPlatformInterface. Timeout waiting for h/w."));

				const float TimeWaited = FPlatformTime::Seconds() - WaitStartTime;
				UE_LOG(LogAudioMixer, Warning, TEXT("AudioMixerPlatformInterface Timeout [%2.f Seconds] waiting for h/w. InstanceID=%d"), TimeWaited,InstanceID);
			}
		}

		OpenStreamParams.AudioMixer->OnAudioStreamShutdown();

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopped;
		return 0;
	}

	uint32 IAudioMixerPlatformInterface::Run()
	{	
		LLM_SCOPE(ELLMTag::AudioMixer);

		uint32 ReturnVal = 0;
		FMemory::SetupTLSCachesOnCurrentThread();

		// Call different functions depending on if it's the "main" audio mixer instance. Helps debugging callstacks.
		if (AudioStreamInfo.AudioMixer->IsMainAudioMixer())
		{
			ReturnVal = MainAudioDeviceRun();
		}
		else
		{
			ReturnVal = RunInternal();
		}

		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
		return ReturnVal;
	}

	/** The default channel orderings to use when using pro audio interfaces while still supporting surround sound. */
	static EAudioMixerChannel::Type DefaultChannelOrder[AUDIO_MIXER_MAX_OUTPUT_CHANNELS];

	static void InitializeDefaultChannelOrder()
	{
		static bool bInitialized = false;
		if (bInitialized)
		{
			return;
		}

		bInitialized = true;

		// Create a hard-coded default channel order
		check(UE_ARRAY_COUNT(DefaultChannelOrder) == AUDIO_MIXER_MAX_OUTPUT_CHANNELS);
		DefaultChannelOrder[0] = EAudioMixerChannel::FrontLeft;
		DefaultChannelOrder[1] = EAudioMixerChannel::FrontRight;
		DefaultChannelOrder[2] = EAudioMixerChannel::FrontCenter;
		DefaultChannelOrder[3] = EAudioMixerChannel::LowFrequency;
		DefaultChannelOrder[4] = EAudioMixerChannel::SideLeft;
		DefaultChannelOrder[5] = EAudioMixerChannel::SideRight;
		DefaultChannelOrder[6] = EAudioMixerChannel::BackLeft;
		DefaultChannelOrder[7] = EAudioMixerChannel::BackRight;

		bool bOverridden = false;
		EAudioMixerChannel::Type ChannelMapOverride[AUDIO_MIXER_MAX_OUTPUT_CHANNELS];
		for (int32 i = 0; i < AUDIO_MIXER_MAX_OUTPUT_CHANNELS; ++i)
		{
			ChannelMapOverride[i] = DefaultChannelOrder[i];
		}

		// Now check the ini file to see if this is overridden
		for (int32 i = 0; i < AUDIO_MIXER_MAX_OUTPUT_CHANNELS; ++i)
		{
			int32 ChannelPositionOverride = 0;

			const TCHAR* ChannelName = EAudioMixerChannel::ToString(DefaultChannelOrder[i]);
			if (GConfig->GetInt(TEXT("AudioDefaultChannelOrder"), ChannelName, ChannelPositionOverride, GEngineIni))
			{
				if (ChannelPositionOverride >= 0 && ChannelPositionOverride < AUDIO_MIXER_MAX_OUTPUT_CHANNELS)
				{
					bOverridden = true;
					ChannelMapOverride[ChannelPositionOverride] = DefaultChannelOrder[i];
				}
				else
				{
					UE_LOG(LogAudioMixer, Error, TEXT("Invalid channel index '%d' in AudioDefaultChannelOrder in ini file."), i);
					bOverridden = false;
					break;
				}
			}
		}

		// Now validate that there's no duplicates.
		if (bOverridden)
		{
			bool bIsValid = true;
			for (int32 i = 0; i < AUDIO_MIXER_MAX_OUTPUT_CHANNELS; ++i)
			{
				for (int32 j = 0; j < AUDIO_MIXER_MAX_OUTPUT_CHANNELS; ++j)
				{
					if (j != i && ChannelMapOverride[j] == ChannelMapOverride[i])
					{
						bIsValid = false;
						break;
					}
				}
			}

			if (!bIsValid)
			{
				UE_LOG(LogAudioMixer, Error, TEXT("Invalid channel index or duplicate entries in AudioDefaultChannelOrder in ini file."));
			}
			else
			{
				for (int32 i = 0; i < AUDIO_MIXER_MAX_OUTPUT_CHANNELS; ++i)
				{
					DefaultChannelOrder[i] = ChannelMapOverride[i];
				}
			}
		}
	}

	bool IAudioMixerPlatformInterface::GetChannelTypeAtIndex(const int32 Index, EAudioMixerChannel::Type& OutType)
	{
		InitializeDefaultChannelOrder();

		if (Index >= 0 && Index < AUDIO_MIXER_MAX_OUTPUT_CHANNELS)
		{
			OutType = DefaultChannelOrder[Index];
			return true;
		}
		return false;
	}

	bool IAudioMixer::ShouldIgnoreDeviceSwaps()
	{
		return DisableDeviceSwapCVar != 0;
	}

	bool IAudioMixer::ShouldLogDeviceSwaps()
	{
		return EnableDetailedWindowsDeviceLoggingCVar != 0;
	}

	bool IAudioMixer::ShouldUseThreadedDeviceSwap()
	{
		return bUseThreadedDeviceSwapCVar != 0;
	}

	bool IAudioMixer::ShouldUseDeviceInfoCache()
	{		
#if PLATFORM_WINDOWS 
		return bUseAudioDeviceInfoCacheCVar != 0;
#else //PLATFORM_WINDOWS
		return false;
#endif //PLATFORM_WINDOWS
	}
	
	bool IAudioMixer::ShouldRecycleThreads()
	{
		return bRecycleThreadsCVar != 0;
	}


}

FAudioPlatformSettings FAudioPlatformSettings::GetPlatformSettings(const TCHAR* PlatformSettingsConfigFile)
{
	FAudioPlatformSettings Settings;

	FString TempString;

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioSampleRate"), TempString, GEngineIni))
	{
		Settings.SampleRate = FMath::Max(FCString::Atoi(*TempString), 8000);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioCallbackBufferFrameSize"), TempString, GEngineIni))
	{
		Settings.CallbackBufferFrameSize = FMath::Max(FCString::Atoi(*TempString), 240);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioNumBuffersToEnqueue"), TempString, GEngineIni))
	{
		Settings.NumBuffers = FMath::Max(FCString::Atoi(*TempString), 1);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioMaxChannels"), TempString, GEngineIni))
	{
		Settings.MaxChannels = FMath::Max(FCString::Atoi(*TempString), 0);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioNumSourceWorkers"), TempString, GEngineIni))
	{
		Settings.NumSourceWorkers = FMath::Max(FCString::Atoi(*TempString), 0);
	}

	return Settings;
}
