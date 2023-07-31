// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DSP/Dsp.h"
#include "EffectConvolutionReverb.h"
#include "Sound/SoundEffectSource.h"
#include "SourceEffectConvolutionReverb.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectConvolutionReverbSettings
{
	GENERATED_USTRUCT_BODY()

	FSourceEffectConvolutionReverbSettings();

	/* Used to account for energy added by convolution with "loud" Impulse Responses. 
	 * This value is not directly editable in the editor because it is copied from the 
	 * associated UAudioImpulseResponse. */
	UPROPERTY();
	float NormalizationVolumeDb;

	// Controls how much of the wet signal is mixed into the output, in Decibels
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset, meta = (ClampMin = "-96.0", ClampMax = "0.0"));
	float WetVolumeDb;

	// Controls how much of the dry signal is mixed into the output, in Decibels
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset, meta = (ClampMin = "-96.0", ClampMax = "0.0"));
	float DryVolumeDb;

	/* If true, input audio is directly routed to output audio with applying any effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SourceEffectPreset)
	bool bBypass;
};


/** Audio render thread effect object. */
class SYNTHESIS_API FSourceEffectConvolutionReverb : public FSoundEffectSource
{
public:
	FSourceEffectConvolutionReverb() = delete;
	// Construct a convolution object with an existing preset. 
	FSourceEffectConvolutionReverb(const USourceEffectConvolutionReverbPreset* InPreset);

	~FSourceEffectConvolutionReverb();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InInitData) override;

	// Called when an audio effect preset settings is changed.
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

	// Call on the game thread in order to update the impulse response and hardware acceleration
	// used in this effect.
	AudioConvReverbIntrinsics::FVersionData UpdateConvolutionReverb(const USourceEffectConvolutionReverbPreset* InPreset);

	void RebuildConvolutionReverb();
private:
	// Sets current runtime settings for convolution reverb which do *not* trigger
	// a FConvolutionReverb rebuild.  These settings will be applied to FConvolutionReverb 
	// at the next call to UpdateParameters()
	void SetConvolutionReverbParameters(const FSourceEffectConvolutionReverbSettings& InSettings);

	// Reverb performs majority of DSP operations
	TSharedRef<Audio::FEffectConvolutionReverb> Reverb;

	float WetVolume = 1.f;
	float DryVolume = 1.f;
	int32 NumChannels = 0;
};

UCLASS()
class SYNTHESIS_API USourceEffectConvolutionReverbPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	
	USourceEffectConvolutionReverbPreset(const FObjectInitializer& ObjectInitializer);

	/* Note: The SourceEffect boilerplate macros could not be utilized here because
	 * the "CreateNewEffect" implementation differed from those available in the
	 * boilerplate macro.
	 */

	virtual bool CanFilter() const override;

	virtual bool HasAssetActions() const;

	virtual FText GetAssetActionName() const override;

	virtual UClass* GetSupportedClass() const override;

	virtual FSoundEffectBase* CreateNewEffect() const override;

	virtual USoundEffectPreset* CreateNewPreset(UObject* InParent, FName Name, EObjectFlags Flags) const override;

	virtual void Init() override;


	FSourceEffectConvolutionReverbSettings GetSettings() const;

	/** Set the convolution reverb settings */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectConvolutionReverbSettings& InSettings);

	/** Set the convolution reverb impulse response */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "Audio|Effects")
	void SetImpulseResponse(UAudioImpulseResponse* InImpulseResponse);

	/** The impulse response used for convolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetImpulseResponse, Category = SourceEffectPreset)
	TObjectPtr<UAudioImpulseResponse> ImpulseResponse;

	/** ConvolutionReverbPreset Preset Settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetSettings, Category = SourceEffectPreset)
	FSourceEffectConvolutionReverbSettings Settings;


	/** Set the internal block size. This can effect latency and performance. Higher values will result in
	 * lower CPU costs while lower values will result higher CPU costs. Latency may be affected depending
	 * on the interplay between audio engines buffer sizes and this effects block size. Generally, higher
	 * values result in higher latency, and lower values result in lower latency. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = SourceEffectPreset)
	ESubmixEffectConvolutionReverbBlockSize BlockSize;

	/** Opt into hardware acceleration of the convolution reverb (if available) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = SourceEffectPreset)
	bool bEnableHardwareAcceleration;
	

#if WITH_EDITORONLY_DATA
	// Binds to the UAudioImpulseRespont::OnObjectPropertyChanged delegate of the current ImpulseResponse
	void BindToImpulseResponseObjectChange();

	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Called when a property changes on the ImpulseResponse object
	void PostEditChangeImpulseProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

	virtual void PostLoad() override;

private:
	void SetImpulseResponseSettings(UAudioImpulseResponse* InImpulseResponse);

	void UpdateSettings();

	void UpdateDeprecatedProperties();

	// This method requires that the effect is registered with a preset.  If this 
	// effect is not registered with a preset, then this will not update the convolution
	// algorithm.
	void RebuildConvolutionReverb();

	mutable FCriticalSection SettingsCritSect; 
	FSourceEffectConvolutionReverbSettings SettingsCopy; 

#if WITH_EDITORONLY_DATA

	TMap<UObject*, FDelegateHandle> DelegateHandles;
#endif
};

