// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Audio.h"
#include "AudioMixer.h"
#include "AudioDevice.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundGenerator.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/MultithreadedPatching.h"
#include "Quartz/AudioMixerClockManager.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"

// Forward Declarations
class FOnSubmixEnvelopeBP;
class IAudioMixerPlatformInterface;
class USoundModulatorBase;
class IAudioLinkFactory;

namespace Audio
{
	// Audio Namespace Forward Declarations
	class FMixerSourceManager;
	class FMixerSourceVoice;
	class FMixerSubmix;
	class FAudioFormatSettings;

	typedef TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> FMixerSubmixPtr;
	typedef TWeakPtr<FMixerSubmix, ESPMode::ThreadSafe> FMixerSubmixWeakPtr;

	/** Data used to schedule events automatically in the audio renderer in audio mixer. */
	struct FAudioThreadTimingData
	{
		/** The time since audio device started. */
		double StartTime;

		/** The clock of the audio thread, periodically synced to the audio render thread time. */
		double AudioThreadTime;

		/** The clock of the audio render thread. */
		double AudioRenderThreadTime;

		/** The current audio thread fraction for audio events relative to the render thread. */
		double AudioThreadTimeJitterDelta;

		FAudioThreadTimingData()
			: StartTime(0.0)
			, AudioThreadTime(0.0)
			, AudioRenderThreadTime(0.0)
			, AudioThreadTimeJitterDelta(0.05)
		{}
	};

	// Master submixes
	namespace EMasterSubmixType
	{
		enum Type
		{
			Master,
			BaseDefault,
			Reverb,
			EQ,
			Count,
		};
	}

	struct FSubmixMap
	{
	public:
		using FObjectId = uint32;
		using FPair = TPair<FObjectId, FMixerSubmixPtr>;
		using FIterFunc = TUniqueFunction<void(const FPair&)>;

		void Add(const FObjectId InObjectId, FMixerSubmixPtr InMixerSubmix);
		void Iterate(FIterFunc InFunction);
		FMixerSubmixPtr FindRef(FObjectId InObjectId);
		int32 Remove(const FObjectId InObjectId);
		void Reset();

	private:
		TMap<FObjectId, FMixerSubmixPtr> SubmixMap;

		FCriticalSection MutationLock;
	};


	class AUDIOMIXER_API FMixerDevice :	public FAudioDevice,
										public IAudioMixer,
										public FGCObject
	{
	public:
		FMixerDevice(IAudioMixerPlatformInterface* InAudioMixerPlatform);
		~FMixerDevice();

		//~ Begin FAudioDevice
		virtual void UpdateDeviceDeltaTime() override;
		virtual void GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const override;
		virtual bool InitializeHardware() override;
		virtual void FadeIn() override;
		virtual void FadeOut() override;
		virtual void TeardownHardware() override;
		virtual void UpdateHardwareTiming() override;
		virtual void UpdateGameThread() override;
		virtual void UpdateHardware() override;
		virtual double GetAudioTime() const override;
		virtual FAudioEffectsManager* CreateEffectsManager() override;
		virtual FSoundSource* CreateSoundSource() override;
		virtual FName GetRuntimeFormat(const USoundWave* SoundWave) const override;
		virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) override;
		virtual bool SupportsRealtimeDecompression() const override;
		virtual bool DisablePCMAudioCaching() const override;
		virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(const USoundWave* SoundWave) const override;
		virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(const FSoundWaveProxyPtr& SoundWave) const override;
		virtual bool ValidateAPICall(const TCHAR* Function, uint32 ErrorCode) override;
		virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
		virtual void CountBytes(class FArchive& Ar) override;
		virtual bool IsExernalBackgroundSoundActive() override;
		virtual void ResumeContext() override;
		virtual void SuspendContext() override;
		virtual void EnableDebugAudioOutput() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual void RegisterSoundSubmix(USoundSubmixBase* SoundSubmix, bool bInit = true) override;
		virtual void UnregisterSoundSubmix(const USoundSubmixBase* SoundSubmix) override;

		virtual void InitSoundEffectPresets() override;
		virtual void InitDefaultAudioBuses() override;
		virtual void ShutdownDefaultAudioBuses() override;
		virtual int32 GetNumActiveSources() const override;

		// Updates the source effect chain (using unique object id). 
		virtual void UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails) override;
		virtual bool GetCurrentSourceEffectChain(const uint32 SourceEffectChainId, TArray<FSourceEffectChainEntry>& OutCurrentSourceEffectChainEntries) override;

		// Submix dry/wet settings
		virtual void UpdateSubmixProperties(USoundSubmixBase* InSubmix) override;
		virtual void SetSubmixWetDryLevel(USoundSubmix* InSoundSubmix, float InOutputVolume, float InWetLevel, float InDryLevel) override;
		virtual void SetSubmixOutputVolume(USoundSubmix* InSoundSubmix, float InOutputVolume) override;
		virtual void SetSubmixWetLevel(USoundSubmix* InSoundSubmix, float InWetLevel) override;
		virtual void SetSubmixDryLevel(USoundSubmix* InSoundSubmix, float InDryLevel) override;

		// Submix auto-disable setteings
		virtual void SetSubmixAutoDisable(USoundSubmix* InSoundSubmix, bool bInAutoDisable) override;
		virtual void SetSubmixAutoDisableTime(USoundSubmix* InSoundSubmix, float InDisableTime) override;

		// Submix Modulation Settings
		virtual void UpdateSubmixModulationSettings(USoundSubmix* InSoundSubmix, const TSet<TObjectPtr<USoundModulatorBase>>& InOutputModulation, const TSet<TObjectPtr<USoundModulatorBase>>& InWetLevelModulation, const TSet<TObjectPtr<USoundModulatorBase>>& InDryLevelModulation) override;
		virtual void SetSubmixModulationBaseLevels(USoundSubmix* InSoundSubmix, float InVolumeModBase, float InWetModBase, float InDryModBase) override;

		// Submix effect chain override settings
		virtual void SetSubmixEffectChainOverride(USoundSubmix* InSoundSubmix, const TArray<FSoundEffectSubmixPtr>& InSubmixEffectPresetChain, float InFadeTimeSec) override;
		virtual void ClearSubmixEffectChainOverride(USoundSubmix* InSoundSubmix, float InFadeTimeSec) override;

		// Submix recording callbacks:
		virtual void StartRecording(USoundSubmix* InSubmix, float ExpectedRecordingDuration) override;
		virtual Audio::FAlignedFloatBuffer& StopRecording(USoundSubmix* InSubmix, float& OutNumChannels, float& OutSampleRate) override;

		virtual void PauseRecording(USoundSubmix* InSubmix);
		virtual void ResumeRecording(USoundSubmix* InSubmix);

		// Submix envelope following
		virtual void StartEnvelopeFollowing(USoundSubmix* InSubmix) override;
		virtual void StopEnvelopeFollowing(USoundSubmix* InSubmix) override;
		virtual void AddEnvelopeFollowerDelegate(USoundSubmix* InSubmix, const FOnSubmixEnvelopeBP& OnSubmixEnvelopeBP) override;

		// Submix Spectrum Analysis
		virtual void StartSpectrumAnalysis(USoundSubmix* InSubmix, const FSoundSpectrumAnalyzerSettings& InSettings) override;
		virtual void StopSpectrumAnalysis(USoundSubmix* InSubmix) override;
		virtual void GetMagnitudesForFrequencies(USoundSubmix* InSubmix, const TArray<float>& InFrequencies, TArray<float>& OutMagnitudes) override;
		virtual void GetPhasesForFrequencies(USoundSubmix* InSubmix, const TArray<float>& InFrequencies, TArray<float>& OutPhases) override;
		virtual void AddSpectralAnalysisDelegate(USoundSubmix* InSubmix, const FSoundSpectrumAnalyzerDelegateSettings& InDelegateSettings, const FOnSubmixSpectralAnalysisBP& OnSubmixSpectralAnalysisBP) override;
		virtual void RemoveSpectralAnalysisDelegate(USoundSubmix* InSubmix, const FOnSubmixSpectralAnalysisBP& OnSubmixSpectralAnalysisBP) override;

		// Submix buffer listener callbacks
		virtual void RegisterSubmixBufferListener(ISubmixBufferListener* InSubmixBufferListener, USoundSubmix* InSubmix = nullptr) override;
		virtual void UnregisterSubmixBufferListener(ISubmixBufferListener* InSubmixBufferListener, USoundSubmix* InSubmix = nullptr) override;

		virtual FPatchOutputStrongPtr AddPatchForSubmix(uint32 InObjectId, float InPatchGain) override;

		virtual void FlushExtended(UWorld* WorldToFlush, bool bClearActivatedReverb);
		virtual void FlushAudioRenderingCommands(bool bPumpSynchronously = false) override;

		// Audio Device Properties
		virtual bool IsNonRealtime() const override;

		//~ End FAudioDevice

		//~ Begin IAudioMixer
		virtual bool OnProcessAudioStream(FAlignedFloatBuffer& OutputBuffer) override;
		virtual void OnAudioStreamShutdown() override;
		//~ End IAudioMixer

		//~ Begin FGCObject
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override
		{
			return TEXT("Audio::FMixerDevice");
		}
		//~End FGCObject

		FMixerSubmixPtr FindSubmixInstanceByObjectId(uint32 InObjectId);

		FMixerSubmixWeakPtr GetSubmixInstance(const USoundSubmixBase* SoundSubmix);

		// If SoundSubmix is a soundfield submix, this will return the factory used to encode 
		// source audio to it's soundfield format.
		// Otherwise, returns nullptr.
		ISoundfieldFactory* GetFactoryForSubmixInstance(USoundSubmix* SoundSubmix);
		ISoundfieldFactory* GetFactoryForSubmixInstance(FMixerSubmixWeakPtr& SoundSubmixPtr);

		// Functions which check the thread it's called on and helps make sure functions are called from correct threads
		void CheckAudioThread() const;
		void CheckAudioRenderingThread() const;
		bool IsAudioRenderingThread() const;

		// Public Functions
		FMixerSourceVoice* GetMixerSourceVoice();
		void ReleaseMixerSourceVoice(FMixerSourceVoice* InSourceVoice);
		int32 GetNumSources() const;

		// AudioLink
		IAudioLinkFactory* GetAudioLinkFactory() const;

		const FAudioPlatformDeviceInfo& GetPlatformDeviceInfo() const { return PlatformInfo; };

		FORCEINLINE int32 GetNumDeviceChannels() const { return PlatformInfo.NumChannels; }

		int32 GetNumOutputFrames() const { return PlatformSettings.CallbackBufferFrameSize; }

		int32 GetNumOutputBuffers() const { return PlatformSettings.NumBuffers; }

		// Retrieve a pointer to the currently active platform. Only use this if you know what you are doing. The returned IAudioMixerPlatformInterface will only be alive as long as this FMixerDevice is alive.
		IAudioMixerPlatformInterface* GetAudioMixerPlatform() const { return AudioMixerPlatform; }

		// Builds a 3D channel map for a spatialized source.
		void Get3DChannelMap(const int32 InSubmixNumChannels, const FWaveInstance* InWaveInstance, const float EmitterAzimuth, const float NormalizedOmniRadius, Audio::FAlignedFloatBuffer& OutChannelMap);

		// Builds a channel gain matrix for a non-spatialized source. The non-static variation of this function queries AudioMixerDevice->NumOutputChannels directly which may not be thread safe.
		void Get2DChannelMap(bool bIsVorbis, const int32 NumSourceChannels, const bool bIsCenterChannelOnly, Audio::FAlignedFloatBuffer& OutChannelMap) const;
		static void Get2DChannelMap(bool bIsVorbis, const int32 NumSourceChannels, const int32 NumOutputChannels, const bool bIsCenterChannelOnly, Audio::FAlignedFloatBuffer& OutChannelMap);

		int32 GetDeviceSampleRate() const;
		int32 GetDeviceOutputChannels() const;

		FMixerSourceManager* GetSourceManager();

		FMixerSubmixWeakPtr GetMasterSubmix(); 
		FMixerSubmixWeakPtr GetBaseDefaultSubmix();
		FMixerSubmixWeakPtr GetMasterReverbSubmix();
		FMixerSubmixWeakPtr GetMasterEQSubmix();

		// Add submix effect to master submix
		void AddMasterSubmixEffect(FSoundEffectSubmixPtr SoundEffect);
		
		// Remove submix effect from master submix
		void RemoveMasterSubmixEffect(uint32 SubmixEffectId);
		
		// Clear all submix effects from master submix
		void ClearMasterSubmixEffects();

		// Add submix effect to given submix
		int32 AddSubmixEffect(USoundSubmix* InSoundSubmix, FSoundEffectSubmixPtr SoundEffect);

		// Remove submix effect to given submix
		void RemoveSubmixEffect(USoundSubmix* InSoundSubmix, uint32 SubmixEffectId);

		// Remove submix effect at the given submix chain index
		void RemoveSubmixEffectAtIndex(USoundSubmix* InSoundSubmix, int32 SubmixChainIndex);

		// Replace the submix effect of the given submix at the submix chain index with the new submix effect id and submix instance
		void ReplaceSoundEffectSubmix(USoundSubmix* InSoundSubmix, int32 InSubmixChainIndex, FSoundEffectSubmixPtr SoundEffect);

		// Clear all submix effects from given submix
		void ClearSubmixEffects(USoundSubmix* InSoundSubmix);

		// Returns the channel array for the given submix channel type
		const TArray<EAudioMixerChannel::Type>& GetChannelArray() const;

		// Retrieves the listener transforms
		const TArray<FTransform>* GetListenerTransforms();

		// Retrieves spherical locations of channels for a given submix format
		const FChannelPositionInfo* GetDefaultChannelPositions() const;

		// Audio thread tick timing relative to audio render thread timing
		double GetAudioThreadTime() const { return AudioThreadTimingData.AudioThreadTime; }
		double GetAudioRenderThreadTime() const { return AudioThreadTimingData.AudioRenderThreadTime; }
		double GetAudioClockDelta() const { return AudioClockDelta; }

		EMonoChannelUpmixMethod GetMonoChannelUpmixMethod() const { return MonoChannelUpmixMethod; }

		TArray<Audio::FChannelPositionInfo>* GetDefaultPositionMap(int32 NumChannels);

		static bool IsEndpointSubmix(const USoundSubmixBase* InSubmix);

		// Audio bus API
		virtual void StartAudioBus(uint32 InAudioBusId, int32 InNumChannels, bool bInIsAutomatic) override;
		virtual void StopAudioBus(uint32 InAudioBusId) override;
		virtual bool IsAudioBusActive(uint32 InAudioBusId) const override;

		virtual FPatchOutputStrongPtr AddPatchForAudioBus(uint32 InAudioBusId, float InPatchGain = 1.0f) override;
		virtual FPatchOutputStrongPtr AddPatchForAudioBus_GameThread(uint32 InAudioBusId, float InPatchGain = 1.0f) override;

		// Clock Manager for quantized event handling on Audio Render Thread
		FQuartzClockManager QuantizedEventClockManager;

		// Keep a reference alive to UQuartzSubsystem state that needs to persist across level transitions (UWorld destruction
		TSharedPtr<FPersistentQuartzSubsystemData, ESPMode::ThreadSafe> QuartzSubsystemData { nullptr };

		// Technically, in editor, multiple UQuartz(World)Subsystem's will reference the same FMixerDevice object.
		// We need to protect around mutation/access of the "shared" state.
		// (in practice this should be low/zero contention)
		FCriticalSection QuartzPersistentStateCritSec;

		// Pushes the command to a audio render thread command queue to be executed on render thread
		void AudioRenderThreadCommand(TFunction<void()> Command);

		// Pushes the command to a MPSC queue to be executed on the game thread
		void GameThreadMPSCCommand(TFunction<void()> InCommand);

	protected:

		virtual void InitSoundSubmixes() override;

		virtual void OnListenerUpdated(const TArray<FListener>& InListeners) override;

		TArray<FTransform> ListenerTransforms;

	private:
		// Resets the thread ID used for audio rendering
		void ResetAudioRenderingThreadId();

		void RebuildSubmixLinks(const USoundSubmixBase& SoundSubmix, FMixerSubmixPtr& SubmixInstance);

		void InitializeChannelMaps();
		static int32 GetChannelMapCacheId(const int32 NumSourceChannels, const int32 NumOutputChannels, const bool bIsCenterChannelOnly);
		void CacheChannelMap(const int32 NumSourceChannels, const int32 NumOutputChannels, const bool bIsCenterChannelOnly);
		void InitializeChannelAzimuthMap(const int32 NumChannels);

		void WhiteNoiseTest(FAlignedFloatBuffer& Output);
		void SineOscTest(FAlignedFloatBuffer& Output);

		bool IsMainAudioDevice() const;

		void LoadMasterSoundSubmix(EMasterSubmixType::Type InType, const FString& InDefaultName, bool bInDefaultMuteWhenBackgrounded, FSoftObjectPath& InOutObjectPath);
		void LoadPluginSoundSubmixes();
		void LoadSoundSubmix(USoundSubmixBase& SoundSubmix);

		void InitSoundfieldAndEndpointDataForSubmix(const USoundSubmixBase& InSoundSubmix, FMixerSubmixPtr MixerSubmix, bool bAllowReInit);

		void UnloadSoundSubmix(const USoundSubmixBase& SoundSubmix);

		ICompressedAudioInfo* CreateAudioInfo(FName InFormat) const;

		bool IsMasterSubmixType(const USoundSubmixBase* InSubmix) const;
		FMixerSubmixPtr GetMasterSubmixInstance(uint32 InSubmixId);
		FMixerSubmixPtr GetMasterSubmixInstance(const USoundSubmixBase* InSubmix);
		
		// Pumps the audio render thread command queue
		void PumpCommandQueue();
		void PumpGameThreadCommandQueue();
		
		TArray<USoundSubmix*> MasterSubmixes;
		TArray<FMixerSubmixPtr> MasterSubmixInstances;

		TArray<TStrongObjectPtr<UAudioBus>> DefaultAudioBuses;

		struct FActiveBusData
		{
			int32 BusId = 0;
			int32 NumChannels = 0;
			bool bIsAutomatic = false;
		};

		// The active audio bus list accessible on the game thread
		TMap<int32, FActiveBusData> ActiveAudioBuses_GameThread;

		/** Ptr to the platform interface, which handles streaming audio to the hardware device. */
		IAudioMixerPlatformInterface* AudioMixerPlatform;
		
		/** Contains a map of channel/speaker azimuth positions. */
		FChannelPositionInfo DefaultChannelAzimuthPositions[EAudioMixerChannel::MaxSupportedChannel];

		/** The azimuth positions for submix channel types. */
		TArray<FChannelPositionInfo> DeviceChannelAzimuthPositions;

		int32 DeviceOutputChannels;

		/** Channel type arrays for submix channel types. */
		TArray<EAudioMixerChannel::Type> DeviceChannelArray;

		/** What upmix method to use for mono channel upmixing. */
		EMonoChannelUpmixMethod MonoChannelUpmixMethod;

		/** What panning method to use for panning. */
		EPanningMethod PanningMethod;

		/** The audio output stream parameters used to initialize the audio hardware. */
		FAudioMixerOpenStreamParams OpenStreamParams;

		/** The time delta for each callback block. */
		double AudioClockDelta;

		/** What the previous master volume was. */
		float PreviousPrimaryVolume;

		/** Timing data for audio thread. */
		FAudioThreadTimingData AudioThreadTimingData;

		/** The platform device info for this mixer device. */
		FAudioPlatformDeviceInfo PlatformInfo;

		/** Map of USoundSubmix static data objects to the dynamic audio mixer submix. */
		FSubmixMap Submixes;

		// Submixes that will sum their audio and send it directly to AudioMixerPlatform.
		// Submixes are added to this list in RegisterSoundSubmix, and removed in UnregisterSoundSubmix.
		TArray<FMixerSubmixPtr> DefaultEndpointSubmixes;

		// Submixes that need to be processed, but will be sending their audio to external sends.
		// Submixes are added to this list in RegisterSoundSubmix and removed in UnregisterSoundSubmix.
		TArray<FMixerSubmixPtr> ExternalEndpointSubmixes;

		// Contended between RegisterSoundSubmix/UnregisterSoundSubmix on the audio thread and OnProcessAudioStream on the audio mixer thread.
		FCriticalSection EndpointSubmixesMutationLock;

		/** Which submixes have been told to envelope follow with this audio device. */
		TArray<USoundSubmix*> DelegateBoundSubmixes;

		/** Queue of mixer source voices. */
		TQueue<FMixerSourceVoice*> SourceVoices;

		TMap<uint32, TArray<FSourceEffectChainEntry>> SourceEffectChainOverrides;

		/** The mixer source manager. */
		TUniquePtr<FMixerSourceManager> SourceManager;

		/** ThreadId for the game thread (or if audio is running a separate thread, that ID) */
		mutable int32 GameOrAudioThreadId;

		/** ThreadId for the low-level platform audio mixer. */
		mutable int32 AudioPlatformThreadId;

		/** Command queue to send commands to audio render thread from game thread or audio thread. */
		TQueue<TFunction<void()>> CommandQueue;

		/** MPSC command queue to send commands to the game thread */
		TMpscQueue<TFunction<void()>> GameThreadCommandQueue;

		IAudioLinkFactory* AudioLinkFactory = nullptr;
		
		/** Whether or not we generate output audio to test multi-platform mixer. */
		bool bDebugOutputEnabled;

		/** Whether or not initialization of the submix system is underway and submixes can be registered */
		bool bSubmixRegistrationDisabled;

	public:

		// Creates a queue for audio decode requests with a specific Id. Tasks
		// created with this Id will not be started immediately upon creation,
		// but will instead be queued up to await a start "kick" later.
		static void CreateSynchronizedAudioTaskQueue(AudioTaskQueueId QueueId);

		// Destroys an audio decode task queue. Tasks currently queued up are 
		// optionally started.
		static void DestroySynchronizedAudioTaskQueue(AudioTaskQueueId QueueId, bool RunCurrentQueue = false);

		// "Kicks" all of the audio decode tasks currentlyt in the queue.
		static int KickQueuedTasks(AudioTaskQueueId QueueId);
	};
}

