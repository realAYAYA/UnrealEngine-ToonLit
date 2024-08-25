// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"
#include "AudioDeviceManager.h"
#include "ISubmixBufferListener.h"
#include "DSP/MultithreadedPatching.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "NiagaraDataInterfaceAudio.generated.h"

/** Class used to to capture the audio stream of an arbitrary submix. */
class FNiagaraSubmixListener : public ISubmixBufferListener
{
public:

	/** Construct an FNiagaraSubmixListener
	 * 
	 * @param InMixer - Input FPatchMixer. Submix audio will be mixed into this patch mixer.
	 * @param InNumSamplesToBuffer - Number of samples to hold in patch input. Low values may cause 
	 *                               data to be overwritten by threads that produce audio. High 
	 *                               values require more memory.
	 */
	NIAGARA_API FNiagaraSubmixListener(Audio::FPatchMixer& InMixer, int32 InNumSamplesToBuffer, Audio::FDeviceId InDeviceId, USoundSubmix* InSoundSubmix);

	FNiagaraSubmixListener(const FNiagaraSubmixListener& Other)
	{
		// Copy constructor technically required to compile TMap, but not used during runtime if move constructor is available.
		// If you're hitting this, consider using Emplace or Add(MoveTemp()).
		checkNoEntry();
	}

	/** Move submix listener. */
	NIAGARA_API FNiagaraSubmixListener(FNiagaraSubmixListener&& Other);

	NIAGARA_API void RegisterToSubmix();

	NIAGARA_API virtual ~FNiagaraSubmixListener();

	/** Returns the current sample rate of the current submix. */
	NIAGARA_API float GetSampleRate() const;

	/** Returns the number of channels of the current submix. */
	NIAGARA_API int32 GetNumChannels() const;

	// Begin ISubmixBufferListener overrides
	NIAGARA_API virtual const FString& GetListenerName() const override;
	NIAGARA_API virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	// End ISubmixBufferListener overrides

private:
	NIAGARA_API FNiagaraSubmixListener();


	NIAGARA_API void UnregisterFromSubmix();

	TAtomic<int32> NumChannelsInSubmix;
	TAtomic<int32> SubmixSampleRate;

	Audio::FPatchInput MixerInput;

	Audio::FDeviceId AudioDeviceId;
	USoundSubmix* Submix;
	bool bIsRegistered;
};

/** Niagara data interface proxy for audio submix listener. */
struct FNiagaraDataInterfaceProxySubmix : public FNiagaraDataInterfaceProxy 
{
	FNiagaraDataInterfaceProxySubmix(const FNiagaraDataInterfaceProxySubmix& Other) = delete;
	FNiagaraDataInterfaceProxySubmix& operator=(const FNiagaraDataInterfaceProxySubmix& Other) = delete;

	/** Construct a FNiagaraDataInterfaceProxySubmix
	 *
	 *  @params InNumSamplesToBuffer - Number of audio sampels to buffer internally. 
	 */
	FNiagaraDataInterfaceProxySubmix(int32 InNumSamplesToBuffer);

	virtual ~FNiagaraDataInterfaceProxySubmix();

	void OnBeginDestroy();

	/** @return the number of channels in the buffer.  */
	int32 GetNumChannels() const;

	/** @return the sample rate of audio in the buffer..  */
	float GetSampleRate() const;

	/** Called when the Submix property changes. */
	virtual void OnUpdateSubmix(USoundSubmix* Submix);

	/** Copies the minimum of NumSamples or however many samples are available into OutBuffer. Returns the number of samples copied, or -1 if this output's corresponding input has been destroyed. */
	int32 PopAudio(float* OutBuffer, int32 NumSamples, bool bUseLatestAudio);

	/** Returns the current number of samples buffered on this output. */
	int32 GetNumSamplesAvailable();

	/** Returns the current number of frames buffered on this output. If NumChannels is zero, then num frames
	 * will also be zero even when samples are available. */
	int32 GetNumFramesAvailable();

	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override;

private:

	void AddSubmixListener(const Audio::FDeviceId InDeviceId);
	void RemoveSubmixListener(const Audio::FDeviceId InDeviceId);

	void RegisterToAllAudioDevices();
	void UnregisterFromAllAudioDevices();

	void OnNewDeviceCreated(Audio::FDeviceId InDeviceId);
	void OnDeviceDestroyed(Audio::FDeviceId InDeviceId);

	// Map of audio devices to submix listeners. Needed for editor where multiple audio devices
	// may exist.
	TMap<Audio::FDeviceId, TSharedPtr<FNiagaraSubmixListener>> SubmixListeners;

	// Mixer for sending audio 
	Audio::FPatchMixer PatchMixer;

	USoundSubmix* SubmixRegisteredTo;
	bool bIsSubmixListenerRegistered;

	int32 NumSamplesToBuffer;
	
	FDelegateHandle DeviceCreatedHandle;
	FDelegateHandle DeviceDestroyedHandle;
};

/** Data Interface allowing sampling of recent audio data. */
UCLASS(EditInlineNew, Category = "Audio", CollapseCategories, meta = (DisplayName = "Audio Submix"), MinimalAPI)
class UNiagaraDataInterfaceAudioSubmix : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
public:
	
	/** The audio submix where analyzed. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	TObjectPtr<USoundSubmix> Submix;


	NIAGARA_API virtual bool Equals(const UNiagaraDataInterface* Other) const override;

#if WITH_EDITOR
	NIAGARA_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	NIAGARA_API virtual void PostInitProperties() override;
	NIAGARA_API virtual void BeginDestroy() override;
	NIAGARA_API virtual void PostLoad() override;

protected:
	NIAGARA_API virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
private:

};

