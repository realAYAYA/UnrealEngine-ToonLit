// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSynesthesiaNRT.h"
#include "OnsetNRT.generated.h"
										 
/** UOnsetNRTSettings
 *
 * Settings for a UOnsetNRT analyzer.
 */
UCLASS(Blueprintable)
class AUDIOSYNESTHESIA_API UOnsetNRTSettings : public UAudioSynesthesiaNRTSettings
{
	GENERATED_BODY()
	public:

		UOnsetNRTSettings();

		/** If true, multichannel audio is downmixed to mono with equal amplitude scaling. If false, each channel gets it's own onset result. */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer)
		bool bDownmixToMono;

		/** Onset timestamp granularity onsets. Lower granularity takes longer to compute. */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer, meta = (ClampMin = "0.005", ClampMax = "0.25"))
		float GranularityInSeconds;

		/** Sensitivity of onset detector. Higher sensitivity will find more onsets. */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer, meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float Sensitivity;

		/** Starting frequency for onset anlaysis. */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer, meta = (ClampMin = "20.0", ClampMax = "20000"))
		float MinimumFrequency;

		/** Starting frequency for onset anlaysis. */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer, meta = (ClampMin = "20.0", ClampMax = "20000"))
		float MaximumFrequency;
		
		/** Convert UOnsetNRTSettings to FOnsetNRTSettings */
		TUniquePtr<Audio::IAnalyzerNRTSettings> GetSettings(const float InSampleRate, const int32 InNumChannels) const;

#if WITH_EDITOR
		virtual FText GetAssetActionName() const override;

		virtual UClass* GetSupportedClass() const override;
#endif
};


/** UOnsetNRT
 *
 * UOnsetNRT calculates the temporal evolution of constant q transform for a given 
 * sound. Onset is available for individual channels or the overall sound asset.
 */
UCLASS(Blueprintable)
class AUDIOSYNESTHESIA_API UOnsetNRT : public UAudioSynesthesiaNRT
{
	GENERATED_BODY()

	public:

		UOnsetNRT();

		/** The settings for the audio analyzer.  */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer)
		TObjectPtr<UOnsetNRTSettings> Settings;

		/** Returns onsets which occured between start and end timestamps. */
		UFUNCTION(BlueprintCallable, Category="Audio Analyzer")
		void GetChannelOnsetsBetweenTimes(const float InStartSeconds, const float InEndSeconds, const int32 InChannel, TArray<float>& OutOnsetTimestamps, TArray<float>& OutOnsetStrengths) const;

		/** Get a specific channel cqt of the analyzed sound at a given time. */
		UFUNCTION(BlueprintCallable, Category="Audio Analyzer")
		void GetNormalizedChannelOnsetsBetweenTimes(const float InStartSeconds, const float InEndSeconds, const int32 InChannel, TArray<float>& OutOnsetTimestamps, TArray<float>& OutOnsetStrengths) const;

		/** Convert ULoudnessNRTSettings to FLoudnessNRTSettings */
 		virtual TUniquePtr<Audio::IAnalyzerNRTSettings> GetSettings(const float InSampleRate, const int32 InNumChannels) const override;

#if WITH_EDITOR
		virtual FText GetAssetActionName() const override;

		virtual UClass* GetSupportedClass() const override;
#endif
	protected:

		/** Return the name of the IAudioAnalyzerNRTFactory associated with this UAudioAnalyzerNRT */
		virtual FName GetAnalyzerNRTFactoryName() const override;

};

