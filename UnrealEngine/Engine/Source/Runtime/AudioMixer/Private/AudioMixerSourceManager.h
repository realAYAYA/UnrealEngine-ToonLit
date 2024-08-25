// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioBusSubsystem.h"
#include "AudioMixerBuffer.h"
#include "AudioMixerBus.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceOutputBuffer.h"
#include "AudioMixerSubmix.h"
#include "AudioMixerTrace.h"
#include "Containers/MpscQueue.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/InterpolatedOnePole.h"
#include "DSP/ParamInterpolator.h"
#include "IAudioExtensionPlugin.h"
#include "ISoundfieldFormat.h"
#include "Sound/SoundModulationDestination.h"
#include "Sound/QuartzQuantizationUtilities.h"
#include "Stats/Stats.h"

#include "AudioMixerSourceManager.generated.h"

// Default this to on (it's quite a small memory footprint).
#ifndef WITH_AUDIO_MIXER_THREAD_COMMAND_DEBUG
	#define WITH_AUDIO_MIXER_THREAD_COMMAND_DEBUG (1)
#endif //WITH_AUDIO_MIXER_THREAD_COMMAND_DEBUG

// Tracks the time it takes to up the source manager (computes source buffers, source effects, sample rate conversion)
DECLARE_CYCLE_STAT_EXTERN(TEXT("Source Manager Update"), STAT_AudioMixerSourceManagerUpdate, STATGROUP_AudioMixer, AUDIOMIXER_API);

// The time it takes to compute the source buffers (handle decoding tasks, resampling)
DECLARE_CYCLE_STAT_EXTERN(TEXT("Source Buffers"), STAT_AudioMixerSourceBuffers, STATGROUP_AudioMixer, AUDIOMIXER_API);

// The time it takes to process the source buffers through their source effects
DECLARE_CYCLE_STAT_EXTERN(TEXT("Source Effect Buffers"), STAT_AudioMixerSourceEffectBuffers, STATGROUP_AudioMixer, AUDIOMIXER_API);

// The time it takes to apply channel maps and get final pre-submix source buffers
DECLARE_CYCLE_STAT_EXTERN(TEXT("Source Output Buffers"), STAT_AudioMixerSourceOutputBuffers, STATGROUP_AudioMixer, AUDIOMIXER_API);

// The time it takes to process the HRTF effect.
DECLARE_CYCLE_STAT_EXTERN(TEXT("HRTF"), STAT_AudioMixerHRTF, STATGROUP_AudioMixer, AUDIOMIXER_API);

// For diagnostics, keep track of what phase of updating the Source manager is in currently.
UENUM()
enum ESourceManagerRenderThreadPhase: uint8
{
	Begin,
	
	PumpMpscCmds,	
	PumpCmds,
	ProcessModulators,
	UpdatePendingReleaseData,
	GenerateSrcAudio_WithBusses,
	ComputeBusses,
	GenerateSrcAudio_WithoutBusses,
	UpdateBusses,
	SpatialInterface_OnAllSourcesProcessed,
	SourceDataOverride_OnAllSourcesProcessed,
	UpdateGameThreadCopies,
	
	Finished,
};

namespace Audio
{
	class FMixerSubmix;
	class FMixerDevice;
	class FMixerSourceVoice;
	class FMixerSourceBuffer;
	class ISourceListener;
	class FMixerSourceSubmixOutputBuffer;

	/** Struct defining a source voice buffer. */
	struct FMixerSourceVoiceBuffer
	{
		/** PCM float data. */
		FAlignedFloatBuffer AudioData;

		/** How many times this buffer will loop. */
		int32 LoopCount = 0;

		/** If this buffer is from real-time decoding and needs to make callbacks for more data. */
		bool bRealTimeBuffer = false;
	};


	class ISourceListener
	{
	public:
		virtual ~ISourceListener() = default;

		// Called before a source begins to generate audio. 
		virtual void OnBeginGenerate() = 0;

		// Called when a loop point is hit
		virtual void OnLoopEnd() = 0;

		// Called when the source finishes on the audio render thread
		virtual void OnDone() = 0;

		// Called when the source's effect tails finish on the audio render thread.
		virtual void OnEffectTailsDone() = 0;

	};

	struct FMixerSourceSubmixSend
	{
		// The submix ptr
		FMixerSubmixWeakPtr Submix;

		// The amount of audio that is to be mixed into this submix
		float SendLevel = 0.0f;

		// Whather or not this is the primary send (i.e. first in the send chain)
		bool bIsMainSend = false;

		// Whether or not this is a pre-distance attenuation send
		EMixerSourceSubmixSendStage SubmixSendStage = EMixerSourceSubmixSendStage::PostDistanceAttenuation;

		// If this is a soundfield submix, this is a pointer to the submix's Soundfield Factory.
		// If this is nullptr, the submix is not a soundfield submix.
		ISoundfieldFactory* SoundfieldFactory = nullptr;
	};

	// Struct holding mappings of bus ids (unique ids) to send level
	struct FInitAudioBusSend
	{
		uint32 AudioBusId = INDEX_NONE;
		float SendLevel = 0.0f;
		int32 BusChannels = 0;
	};

	struct FMixerSourceVoiceInitParams
	{
		TSharedPtr<FMixerSourceBuffer, ESPMode::ThreadSafe> MixerSourceBuffer = nullptr;
		ISourceListener* SourceListener = nullptr;
		TArray<FMixerSourceSubmixSend> SubmixSends;
		TArray<FInitAudioBusSend> AudioBusSends[(int32)EBusSendType::Count];
		uint32 AudioBusId = INDEX_NONE;
		int32 AudioBusChannels = 0;
		float SourceBusDuration = 0.0f;
		uint32 SourceEffectChainId = INDEX_NONE;
		TArray<FSourceEffectChainEntry> SourceEffectChain;
		int32 SourceEffectChainMaxSupportedChannels = 0;
		FMixerSourceVoice* SourceVoice = nullptr;
		int32 NumInputChannels = 0;
		int32 NumInputFrames = 0;
		float EnvelopeFollowerAttackTime = 10.0f;
		float EnvelopeFollowerReleaseTime = 100.0f;
		FString DebugName;
		USpatializationPluginSourceSettingsBase* SpatializationPluginSettings = nullptr;
		UOcclusionPluginSourceSettingsBase* OcclusionPluginSettings = nullptr;
		UReverbPluginSourceSettingsBase* ReverbPluginSettings = nullptr;
		USourceDataOverridePluginSourceSettingsBase* SourceDataOverridePluginSettings = nullptr;

		FSoundModulationDefaultSettings ModulationSettings;

		FQuartzQuantizedRequestData QuantizedRequestData;

		FSharedISourceBufferListenerPtr SourceBufferListener;

		IAudioLinkFactory::FAudioLinkSourcePushedSharedPtr AudioLink;

		FName AudioComponentUserID;
		uint64 AudioComponentID = 0;
		bool bIs3D = false;
		bool bPlayEffectChainTails = false;
		bool bUseHRTFSpatialization = false;
		bool bIsExternalSend = false;
		bool bIsDebugMode  = false;
		bool bEnableBusSends = false;
		bool bEnableBaseSubmix = false;
		bool bEnableSubmixSends = false;
		bool bIsVorbis = false;
		bool bIsSoundfield = false;
		bool bIsSeeking = false;
		bool bShouldSourceBufferListenerZeroBuffer = false;

		uint32 PlayOrder = INDEX_NONE;
	};

	struct FSourceManagerInitParams
	{
		// Total number of sources to use in the source manager
		int32 NumSources = 0;

		// Number of worker threads to use for the source manager.
		int32 NumSourceWorkers = 0;
	};

	class FMixerSourceManager
	{
	public:
		FMixerSourceManager(FMixerDevice* InMixerDevice);
		~FMixerSourceManager();

		void Init(const FSourceManagerInitParams& InitParams);
		void Update(bool bTimedOut = false);

		bool GetFreeSourceId(int32& OutSourceId);
		int32 GetNumActiveSources() const;
		int32 GetNumActiveAudioBuses() const;

		void ReleaseSourceId(const int32 SourceId);
		void InitSource(const int32 SourceId, const FMixerSourceVoiceInitParams& InitParams);

		// Creates and starts an audio bus manually.
		void StartAudioBus(FAudioBusKey InAudioBusKey, int32 InNumChannels, bool bInIsAutomatic);

		// Stops an audio bus manually
		void StopAudioBus(FAudioBusKey InAudioBusKey);

		// Queries if an audio bus is active. Must be called from the audio thread.
		bool IsAudioBusActive(FAudioBusKey InAudioBusKey) const;

		// Returns the number of channels currently set for the audio bus associated with
		// the provided BusId.  Returns 0 if the audio bus is inactive.
		int32 GetAudioBusNumChannels(FAudioBusKey InAudioBusKey) const;

		// Adds a patch output for an audio bus from the Audio Render Thread
		void AddPatchOutputForAudioBus(FAudioBusKey InAudioBusKey, const FPatchOutputStrongPtr& InPatchOutputStrongPtr);

		// Adds a patch output for an audio bus from the Audio Thread
		void AddPatchOutputForAudioBus_AudioThread(FAudioBusKey InAudioBusKey, const FPatchOutputStrongPtr& InPatchOutputStrongPtr);

		// Adds a patch input for an audio bus
		void AddPatchInputForAudioBus(FAudioBusKey InAudioBusKey, const FPatchInput& InPatchInput);

		// Adds a patch input for an audio bus from the Audio Thread
		void AddPatchInputForAudioBus_AudioThread(FAudioBusKey InAudioBusKey, const FPatchInput& InPatchInput);

		void Play(const int32 SourceId);
		void Stop(const int32 SourceId);
		void CancelQuantizedSound(const int32 SourceId);
		void StopInternal(const int32 SourceId);
		void StopFade(const int32 SourceId, const int32 NumFrames);
		void Pause(const int32 SourceId);
		void SetPitch(const int32 SourceId, const float Pitch);
		void SetVolume(const int32 SourceId, const float Volume);
		void SetDistanceAttenuation(const int32 SourceId, const float DistanceAttenuation);
		void SetSpatializationParams(const int32 SourceId, const FSpatializationParams& InParams);
		void SetChannelMap(const int32 SourceId, const uint32 NumInputChannels, const Audio::FAlignedFloatBuffer& InChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly);
		void SetLPFFrequency(const int32 SourceId, const float Frequency);
		void SetHPFFrequency(const int32 SourceId, const float Frequency);

		// Sets base (i.e. carrier) frequency of modulatable parameters
		void SetModPitch(const int32 SourceId, const float InModPitch);
		void SetModVolume(const int32 SourceId, const float InModVolume);
		void SetModLPFFrequency(const int32 SourceId, const float InModFrequency);
		void SetModHPFFrequency(const int32 SourceId, const float InModFrequency);
		
		void SetModulationRouting(const int32 SourceId, FSoundModulationDefaultSettings& ModulationSettings);

		void SetSourceBufferListener(const int32 SourceId, FSharedISourceBufferListenerPtr& InSourceBufferListener, bool InShouldSourceBufferListenerZeroBuffer);

		void SetListenerTransforms(const TArray<FTransform>& ListenerTransforms);
		const TArray<FTransform>* GetListenerTransforms() const;

		int64 GetNumFramesPlayed(const int32 SourceId) const;
		float GetEnvelopeValue(const int32 SourceId) const;
#if ENABLE_AUDIO_DEBUG
		double GetCPUCoreUtilization(const int32 SourceId) const;
#endif // ENABLE_AUDIO_DEBUG
		bool IsUsingHRTFSpatializer(const int32 SourceId) const;
		bool NeedsSpeakerMap(const int32 SourceId) const;
		void ComputeNextBlockOfSamples();
		void ClearStoppingSounds();
		void MixOutputBuffers(const int32 SourceId, int32 InNumOutputChannels, const float InSendLevel, EMixerSourceSubmixSendStage InSubmixSendStage, FAlignedFloatBuffer& OutWetBuffer) const;

		// Retrieves a channel map for the given source ID for the given output channels
		// can be used even when a source is 3D if the source is doing any kind of bus sending or otherwise needs a channel map
		void Get2DChannelMap(const int32 SourceId, int32 InNumOutputChannels, Audio::FAlignedFloatBuffer& OutChannelMap);

		// Called by a soundfield submix to get encoded audio.
		// If this source wasn't encoded (possibly because it is paused or finished playing),
		// this returns nullptr.
		// Returned nonnull pointers are only guaranteed to be valid on the audio mixer render thread.
		const ISoundfieldAudioPacket* GetEncodedOutput(const int32 SourceId, const FSoundfieldEncodingKey& InKey) const;

		const FQuat GetListenerRotation(const int32 SourceId) const;

		void SetSubmixSendInfo(const int32 SourceId, const FMixerSourceSubmixSend& SubmixSend);
		void ClearSubmixSendInfo(const int32 SourceId, const FMixerSourceSubmixSend& SubmixSend);

		void SetBusSendInfo(const int32 SourceId, EBusSendType InAudioBusSendType, uint32 AudiobusId, float BusSendLevel);

		void UpdateDeviceChannelCount(const int32 InNumOutputChannels);

		void UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails);


		// Quantized event methods
		void PauseSoundForQuantizationCommand(const int32 SourceId);
		void SetSubBufferDelayForSound(const int32 SourceId, const int32 FramesToDelay);
		void UnPauseSoundForQuantizationCommand(const int32 SourceId);

		// Buffer getters
		const float* GetPreDistanceAttenuationBuffer(const int32 SourceId) const;
		const float* GetPreEffectBuffer(const int32 SourceId) const;
		const float* GetPreviousSourceBusBuffer(const int32 SourceId) const;
		const float* GetPreviousAudioBusBuffer(const int32 AudioBusId) const;
		int32 GetNumChannels(const int32 SourceId) const;
		int32 GetNumOutputFrames() const { return NumOutputFrames; }
		bool IsSourceBus(const int32 SourceId) const;
		void PumpCommandQueue();
		void UpdatePendingReleaseData(bool bForceWait = false);
		void FlushCommandQueue(bool bPumpCommandQueue = false);

		// Pushes a TFUnction command into an MPSC queue from an arbitrary thread to the audio render thread
		void AudioMixerThreadMPSCCommand(TFunction<void()>&& InCommand, const char* InDebugString=nullptr);
		
		void AddPendingAudioBusConnection(FAudioBusKey AudioBusKey, int32 NumChannels, bool bIsAutomatic, FPatchInput PatchInput)
		{
			PendingAudioBusConnections.Enqueue(FPendingAudioBusConnection{ FPendingAudioBusConnection::FPatchVariant(TInPlaceType<FPatchInput>(), MoveTemp(PatchInput)), MoveTemp(AudioBusKey), NumChannels, bIsAutomatic });
		}

		void AddPendingAudioBusConnection(FAudioBusKey AudioBusKey, int32 NumChannels, bool bIsAutomatic, FPatchOutputStrongPtr PatchOutputStrongPtr)
		{
			PendingAudioBusConnections.Enqueue(FPendingAudioBusConnection{ FPendingAudioBusConnection::FPatchVariant(TInPlaceType<FPatchOutputStrongPtr>(), MoveTemp(PatchOutputStrongPtr)), MoveTemp(AudioBusKey), NumChannels, bIsAutomatic });
		}

	private:
#define INVALID_AUDIO_RENDER_THREAD_ID static_cast<uint32>(-1)
		uint32 AudioRenderThreadId = INVALID_AUDIO_RENDER_THREAD_ID;
		void ReleaseSource(const int32 SourceId);
		void BuildSourceEffectChain(const int32 SourceId, FSoundEffectSourceInitData& InitData, const TArray<FSourceEffectChainEntry>& SourceEffectChain, TArray<TSoundEffectSourcePtr>& OutSourceEffects);
		void ResetSourceEffectChain(const int32 SourceId);
		void ReadSourceFrame(const int32 SourceId);

		void GenerateSourceAudio(const bool bGenerateBuses);
		void GenerateSourceAudio(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);

		void ComputeSourceBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);
		void ComputePostSourceEffectBufferForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);
		void ComputeOutputBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);

		void ConnectBusPatches();
		void ComputeBuses();
		void UpdateBuses();

		struct FAudioMixerThreadCommand
		{
			// ctor
			FAudioMixerThreadCommand() = default;
			FAudioMixerThreadCommand(TFunction<void()>&& InFunction, const char* InDebugString, bool bInDeferExecution = false);
			
			// function-call operator
			void operator()() const;

			// data
			TFunction<void()> Function;

			// Defers the execution by a single call to PumpCommandQueue()
			// (used for commands that affect a playing source,
			// and that source gets initialized after the command executes
			bool bDeferExecution = false;

#if WITH_AUDIO_MIXER_THREAD_COMMAND_DEBUG			
			const char* DebugString=nullptr;					// Statically defined string from macro AUDIO_MIXER_THREAD_COMMAND_STRING
			mutable uint64_t StartExecuteTimeInCycles=0;		// Set just before Function is called, for diagnostics.
#endif // #if WITH_AUDIO_MIXER_THREAD_COMMAND_DEBUG

			FString GetSafeDebugString() const;
			float GetExecuteTimeInSeconds() const;
		};

		void AudioMixerThreadCommand(TFunction<void()>&& InFunction, const char* InDebugString = nullptr, bool bInDeferExecution = false);

		static const int32 NUM_BYTES_PER_SAMPLE = 2;

		// Private class which perform source buffer processing in a worker task
		class FAudioMixerSourceWorker : public FNonAbandonableTask
		{
			FMixerSourceManager* SourceManager;
			int32 StartSourceId;
			int32 EndSourceId;
			bool bGenerateBuses;

		public:
			FAudioMixerSourceWorker(FMixerSourceManager* InSourceManager, const int32 InStartSourceId, const int32 InEndSourceId)
				: SourceManager(InSourceManager)
				, StartSourceId(InStartSourceId)
				, EndSourceId(InEndSourceId)
				, bGenerateBuses(false)
			{
			}

			void SetGenerateBuses(bool bInGenerateBuses)
			{
				bGenerateBuses = bInGenerateBuses;
			}

			void DoWork()
			{
				SourceManager->GenerateSourceAudio(bGenerateBuses, StartSourceId, EndSourceId);
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FAudioMixerSourceWorker, STATGROUP_ThreadPoolAsyncTasks);
			}
		};

		// Critical section to ensure mutating effect chains is thread-safe
		FCriticalSection EffectChainMutationCriticalSection;

		FMixerDevice* MixerDevice;

		// Info about spatialization plugin
		FAudioDevice::FAudioSpatializationInterfaceInfo SpatialInterfaceInfo;
		
		// Cached ptr to an optional source data override plugin
		TAudioSourceDataOverridePtr SourceDataOverridePlugin;

		IAudioLinkFactory* AudioLinkFactory = nullptr;

		// Array of pointers to game thread audio source objects
		TArray<FMixerSourceVoice*> MixerSources;

		// A command queue to execute commands from audio thread (or game thread) to audio mixer device thread.
		struct FCommands
		{
			FThreadSafeCounter NumTimesOvergrown = 0;
			TArray<FAudioMixerThreadCommand> SourceCommandQueue;
		};
		
		FCommands CommandBuffers[2];
		FThreadSafeCounter RenderThreadCommandBufferIndex;

		FEvent* CommandsProcessedEvent;
		FCriticalSection CommandBufferIndexCriticalSection;

		TArray<int32> DebugSoloSources;

		using FAudioMixerMpscCommand = FAudioMixerThreadCommand;
		TMpscQueue<FAudioMixerMpscCommand> MpscCommandQueue;
		
		struct FSourceInfo
		{
			FSourceInfo() {}
			~FSourceInfo() {}

			// Object which handles source buffer decoding
			TSharedPtr<FMixerSourceBuffer, ESPMode::ThreadSafe> MixerSourceBuffer;
			ISourceListener* SourceListener;

			// Data used for rendering sources
			TSharedPtr<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe> CurrentPCMBuffer;
			int32 CurrentAudioChunkNumFrames;

			// The post-attenuation source buffer, used to send audio to submixes
			Audio::FAlignedFloatBuffer SourceBuffer;
			Audio::FAlignedFloatBuffer PreEffectBuffer;
			Audio::FAlignedFloatBuffer PreDistanceAttenuationBuffer;
			Audio::FAlignedFloatBuffer SourceEffectScratchBuffer;

			// Data used for delaying the rendering of source audio for sample-accurate quantization
			int32 SubCallbackDelayLengthInFrames{ 0 };
			Audio::TCircularAudioBuffer<float> SourceBufferDelayLine;

			TArray<float> CurrentFrameValues;
			TArray<float> NextFrameValues;
			float CurrentFrameAlpha;
			int32 CurrentFrameIndex;
			int64 NumFramesPlayed;

			// The number of frames to wait before starting the source
			double StartTime;

			TArray<FMixerSourceSubmixSend> SubmixSends;

			// What audio bus Id this source is sonfiying, if it is a source bus. This is INDEX_NONE for sources which are not source buses.
			uint32 AudioBusId;

			// Number of samples to count for source bus
			int64 SourceBusDurationFrames;

			// What buses this source is sending its audio to. Used to remove this source from the bus send list.
			TArray<uint32> AudioBusSends[(int32)EBusSendType::Count];

			// Interpolated source params
			FParam PitchSourceParam;
			float VolumeSourceStart;
			float VolumeSourceDestination;
			float VolumeFadeSlope;
			float VolumeFadeStart;
			int32 VolumeFadeFramePosition;
			int32 VolumeFadeNumFrames;

			float DistanceAttenuationSourceStart;
			float DistanceAttenuationSourceDestination;

			// Legacy filter LFP & HPF frequency set directly (not by modulation) on source
			float LowPassFreq;
			float HighPassFreq;

			// One-Pole LPFs and HPFs per source
			Audio::FInterpolatedLPF LowPassFilter;
			Audio::FInterpolatedHPF HighPassFilter;

			// Source effect instances
			uint32 SourceEffectChainId;
			TArray<TSoundEffectSourcePtr> SourceEffects;
			TArray<USoundEffectSourcePreset*> SourceEffectPresets;
			bool bEffectTailsDone;
			FSoundEffectSourceInputData SourceEffectInputData;

			FAudioPluginSourceOutputData AudioPluginOutputData;

			// A DSP object which tracks the amplitude envelope of a source.
			Audio::FInlineEnvelopeFollower SourceEnvelopeFollower;
			float SourceEnvelopeValue;

			// Modulation destinations
			Audio::FModulationDestination VolumeModulation;
			Audio::FModulationDestination PitchModulation;
			Audio::FModulationDestination LowpassModulation;
			Audio::FModulationDestination HighpassModulation;

			// Modulation Base (i.e. Carrier) Values
			float VolumeModulationBase;
			float PitchModulationBase;
			float LowpassModulationBase;
			float HighpassModulationBase;

			FSpatializationParams SpatParams;
			Audio::FAlignedFloatBuffer ScratchChannelMap;

			// Quantization data
			FQuartzQuantizedCommandHandle QuantizedCommandHandle;

			// Optional Source buffer listener.
			FSharedISourceBufferListenerPtr SourceBufferListener;

			// Optional AudioLink.
			IAudioLinkFactory::FAudioLinkSourcePushedSharedPtr AudioLink;

			// State management
			uint8 bIs3D:1;
			uint8 bIsCenterChannelOnly:1;
			uint8 bIsActive:1;
			uint8 bIsPlaying:1;
			uint8 bIsPaused:1;
			uint8 bIsPausedForQuantization:1;
			uint8 bDelayLineSet:1;
			uint8 bIsStopping:1;
			uint8 bHasStarted:1;
			uint8 bIsBusy:1;
			uint8 bUseHRTFSpatializer:1;
			uint8 bIsExternalSend:1;
			uint8 bUseOcclusionPlugin:1;
			uint8 bUseReverbPlugin:1;
			uint8 bIsDone:1;
			uint8 bIsLastBuffer:1;
			uint8 bEnableBusSends : 1;
			uint8 bEnableBaseSubmix : 1;
			uint8 bEnableSubmixSends : 1;
			uint8 bIsVorbis:1;
			uint8 bIsSoundfield:1;
			uint8 bHasPreDistanceAttenuationSend:1;
			uint8 bModFiltersUpdated : 1;
			uint8 bShouldSourceBufferListenerZeroBuffer : 1;

			// Source format info
			int32 NumInputChannels;
			int32 NumPostEffectChannels;
			int32 NumInputFrames;

			uint32 PlayOrder;

			// ID for associated Audio Component if there is one, 0 otherwise
			uint64 AudioComponentID;

			FORCEINLINE void ResetModulators(const Audio::FDeviceId InDeviceId)
			{
				VolumeModulation.Init(InDeviceId, FName("Volume"), false /* bInIsBuffered */, true /* bInValueLinear */);
				PitchModulation.Init(InDeviceId, FName("Pitch"));
				HighpassModulation.Init(InDeviceId, FName("HPFCutoffFrequency"));
				LowpassModulation.Init(InDeviceId, FName("LPFCutoffFrequency"));

				VolumeModulationBase = 0.0f;
				PitchModulationBase = 0.0f;
				HighpassModulationBase = MIN_FILTER_FREQUENCY;
				LowpassModulationBase = MAX_FILTER_FREQUENCY;
			}

			//Helper function for determining if OutputToBusOnly is enabled
			bool IsRenderingToSubmixes() const;

#if AUDIO_MIXER_ENABLE_DEBUG_MODE
			uint8 bIsDebugMode : 1;
			FString DebugName;
#endif // AUDIO_MIXER_ENABLE_DEBUG_MODE
		};

		static void ApplyDistanceAttenuation(FSourceInfo& InSourceInfo, int32 NumSamples);
		void ComputePluginAudio(FSourceInfo& InSourceInfo, FMixerSourceSubmixOutputBuffer& InSourceSubmixOutputBuffer, int32 SourceId, int32 NumSamples);

		// Hang/crash diagnostics.
		void DoStallDiagnostics();

		void LogRenderThreadStall();
		void LogInflightAsyncTasks();
		void LogCallstacks();
		void LogCallstack(uint32 InThreadId);

		// Array of listener transforms
		TArray<FTransform> ListenerTransforms;

		// Array of source infos.
		TArray<FSourceInfo> SourceInfos;

		// This array is independent of SourceInfos array to optimize for cache coherency
		TArray<FMixerSourceSubmixOutputBuffer> SourceSubmixOutputBuffers;

		// Map of bus object Id's to audio bus data. 
		TMap<FAudioBusKey, TSharedPtr<FMixerAudioBus>> AudioBuses; 
		TArray<FAudioBusKey> AudioBusKeys_AudioThread;

		// Async task workers for processing sources in parallel
		TArray<FAsyncTask<FAudioMixerSourceWorker>*> SourceWorkers;

		// Array of task data waiting to finished. Processed on audio render thread.
		TArray<TSharedPtr<FMixerSourceBuffer, ESPMode::ThreadSafe>> PendingSourceBuffers;

		// General information about sources in source manager accessible from game thread
		struct FGameThreadInfo
		{
			TArray<int32> FreeSourceIndices;
			TArray<bool> bIsBusy;
			TArray<bool> bNeedsSpeakerMap;
			TArray<bool> bIsDebugMode;
			TArray<bool> bIsUsingHRTFSpatializer;
#if ENABLE_AUDIO_DEBUG
			TArray<double> CPUCoreUtilization;
#endif // #if ENABLE_AUDIO_DEBUG
		} GameThreadInfo;

		int32 NumActiveSources;
		int32 NumTotalSources;
		int32 NumOutputFrames;
		int32 NumOutputSamples;
		int32 NumSourceWorkers;

		// Commands queued up to execute
		FThreadSafeCounter NumCommands;

		uint8 bInitialized : 1;
		uint8 bUsingSpatializationPlugin : 1;
		uint8 bUsingSourceDataOverridePlugin : 1;

		// Set to true when the audio source manager should pump the command queue
		FThreadSafeBool bPumpQueue;
		std::atomic<uint64> LastPumpCompleteTimeInCycles=0;
		std::atomic<ESourceManagerRenderThreadPhase> RenderThreadPhase=ESourceManagerRenderThreadPhase::Begin;
		FRWLock CurrentlyExecutingCmdLock;						// R/W slim lock for the currently executing cmd, so we can safely query it.
		FAudioMixerThreadCommand CurrentlyExecuteingCmd;		// Keep this as a member so we can't always peek the executing cmd.

		struct FPendingAudioBusConnection
		{
			using FPatchVariant = TVariant<FPatchInput, FPatchOutputStrongPtr>;
			FPatchVariant PatchVariant;
			FAudioBusKey AudioBusKey;
			int32 NumChannels = 0;
			bool bIsAutomatic = false;
		};

		TMpscQueue<FPendingAudioBusConnection> PendingAudioBusConnections;

		friend class FMixerSourceVoice;
	};
}
