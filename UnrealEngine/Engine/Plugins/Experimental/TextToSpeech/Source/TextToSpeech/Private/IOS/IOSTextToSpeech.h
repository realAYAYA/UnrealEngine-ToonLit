// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#if PLATFORM_IOS
#include "CoreMinimal.h"
#include "GenericPlatform/TextToSpeechBase.h"

@class AVSpeechSynthesizer;
@class FSpeechSynthesizerDelegate;

class FIOSTextToSpeech : public FTextToSpeechBase
{
public:
	FIOSTextToSpeech();
	virtual ~FIOSTextToSpeech();
	
	// FGenericTextToSpeech
	virtual void Speak(const FString& InStringToSpeak) override final;
	virtual bool IsSpeaking() const override final;
	virtual void StopSpeaking() override final;
	virtual float GetVolume() const override final;
	virtual void SetVolume(float InVolume) override final;
	virtual float GetRate() const override final;
	virtual void SetRate(float InRate) override final;
	virtual void Mute() override final;
	virtual void Unmute() override final;
protected:
	virtual void OnActivated() override final;
	virtual void OnDeactivated() override final;
	// ~
private:
	/** True if the speech synthesizer *is speaking. Else false. */
	bool bIsSpeaking;
	/** The current volume all TTS utterances will be played at **/
	float Volume;
	/** The speech rate all TTS utterances will be made at. */
	float Rate;
	/** The platform speech synthesizer that converts text to speech */
	AVSpeechSynthesizer* SpeechSynthesizer;
	/**
	* The delegate for the speech synthesizer. Callbacks indicating
	* speech synthesis playback progress, intruuption and completion are handled by this delegate.
	 */
	FSpeechSynthesizerDelegate* SpeechSynthesizerDelegate;
};
#endif

