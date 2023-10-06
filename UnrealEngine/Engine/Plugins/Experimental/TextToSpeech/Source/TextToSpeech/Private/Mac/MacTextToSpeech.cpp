// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_MAC
#include "Mac/MacTextToSpeech.h"
#include "GenericPlatform/TextToSpeechBase.h"
#include "TextToSpeechLog.h"
#include "Mac/CocoaThread.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

/** Defined in FTextToSpeechBase.cpp */
extern TMap<TextToSpeechId, TWeakPtr<FTextToSpeechBase>> ActiveTextToSpeechMap;

@interface FSpeechSynthesizerDelegate : FApplePlatformObject<NSSpeechSynthesizerDelegate>
-(id)initWithTextToSpeechId:(TextToSpeechId)InOwningTextToSpeechId;
-(void)dealloc;
- (void)speechSynthesizer:(NSSpeechSynthesizer *)sender didFinishSpeaking:(BOOL)finishedSpeaking;
@end

@implementation FSpeechSynthesizerDelegate
{
	TextToSpeechId _OwningTextToSpeechId;
}
APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FSpeechSynthesizerDelegate)
-(id)initWithTextToSpeechId:(TextToSpeechId)InOwningTextToSpeechId
{
	if (self = [super init])
	{
		_OwningTextToSpeechId = InOwningTextToSpeechId;
	}
	return self;
}

-(void)dealloc
{
	_OwningTextToSpeechId = FTextToSpeechBase::InvalidTextToSpeechId;
	[super dealloc];
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender didFinishSpeaking:(BOOL)finishedSpeaking
{
	// The announcement was completed successfully, not interrupted or manually stopped
if (finishedSpeaking && _OwningTextToSpeechId != FTextToSpeechBase::InvalidTextToSpeechId)
{
	TextToSpeechId IdCopy = _OwningTextToSpeechId;
	const TMap<TextToSpeechId, TWeakPtr<FTextToSpeechBase>>& ActiveTextToSpeechMapRef = ActiveTextToSpeechMap;
	GameThreadCall(^{
		const TWeakPtr<FTextToSpeechBase>* TTSPtr = ActiveTextToSpeechMapRef.Find(IdCopy);
		if (TTSPtr && TTSPtr->IsValid())
		{
			TTSPtr->Pin()->OnTextToSpeechFinishSpeaking_GameThread();
		}
	}, @[ NSDefaultRunLoopMode ], false);
}
}

@end

const float FMacTextToSpeech::MinimumRateWPM = 100.0f;
const float FMacTextToSpeech::MaximumRateWPM = 600.0f;

FMacTextToSpeech::FMacTextToSpeech()
	: bIsSpeaking(false)
	, Volume(0.0f)
	, Rate(0.0f)
	, SpeechSynthesizer(nullptr)
	, SpeechSynthesizerDelegate(nullptr)
{

}

FMacTextToSpeech::~FMacTextToSpeech()
{
	// base class already takes care of checking if the TTS is active and calls deactivate
}

void FMacTextToSpeech::Speak(const FString& InStringToSpeak)
{
	if (IsActive())
	{
		UE_LOG(LogTextToSpeech, Verbose, TEXT("Mac TTS speak requested."));
		if (!InStringToSpeak.IsEmpty())
		{
			UE_LOG(LogTextToSpeech, VeryVerbose, TEXT("String to speak: %s"), *InStringToSpeak);
			if (IsSpeaking())
			{
				StopSpeaking();
			}
			SCOPED_AUTORELEASE_POOL;
			NSString* Announcement = InStringToSpeak.GetNSString();
			[SpeechSynthesizer startSpeakingString:Announcement];
			bIsSpeaking = true;
		}
	}
}

bool FMacTextToSpeech::IsSpeaking() const
{
	if (IsActive())
	{
		return bIsSpeaking;
	}
	return false;
}

void FMacTextToSpeech::StopSpeaking()
{
	if (IsActive())
	{
		[SpeechSynthesizer stopSpeaking];
		bIsSpeaking = false;
		UE_LOG(LogTextToSpeech, Verbose, TEXT("Mac TTS stopped speaking."));
	}
}

float FMacTextToSpeech::GetVolume() const
{
	return Volume;
}

void FMacTextToSpeech::SetVolume(float InVolume)
{
	Volume = FMath::Clamp(InVolume, 0.f, 1.0f);
	if (!IsMuted())
	{
		SpeechSynthesizer.volume = Volume;
	}
}

float FMacTextToSpeech::GetRate() const
{
	return Rate;
}

void FMacTextToSpeech::SetRate(float InRate)
{
	Rate = FMath::Clamp(InRate, 0.0f, 1.0f);
	// speech synthesizer only accepts rate in words per minute
	// we do the conversion here
	SpeechSynthesizer.rate = RateToWPM(Rate);
}

float FMacTextToSpeech::RateToWPM(float InRate) const
{
	ensure(InRate >= 0 && InRate <= 1.0f);
	return FMacTextToSpeech::MinimumRateWPM + (FMacTextToSpeech::MaximumRateWPM - FMacTextToSpeech::MinimumRateWPM) * InRate;
}

void FMacTextToSpeech::Mute()
{
	if (IsActive() && !IsMuted())
	{
		SetMuted(true);
		SpeechSynthesizer.volume = 0;
	}
}

void FMacTextToSpeech::Unmute()
{
	if (IsActive() && IsMuted())
	{
		SetMuted(false);
		SpeechSynthesizer.volume = Volume;
	}
}

float FMacTextToSpeech::RateWPMToRate(float InRateWPM) const
{
	ensure(InRateWPM >= FMacTextToSpeech::MinimumRateWPM && InRateWPM <= FMacTextToSpeech::MaximumRateWPM);
	return InRateWPM / (FMacTextToSpeech::MaximumRateWPM - FMacTextToSpeech::MinimumRateWPM);
}

void FMacTextToSpeech::OnActivated()
{
	ensureMsgf(!IsActive(), TEXT("Attempting to activate an already activated TTS. FTextToSpeechBase::Activate() should already guard against this."));
	TextToSpeechId IdCopy = GetId();
	MainThreadCall(^{
		SpeechSynthesizer = [[NSSpeechSynthesizer alloc] init];
		checkf(SpeechSynthesizer, TEXT("Failed to create Mac speech synthesizer."));
		SpeechSynthesizerDelegate = [[FSpeechSynthesizerDelegate alloc] initWithTextToSpeechId:IdCopy];
		checkf(SpeechSynthesizerDelegate, TEXT("Failed to create Mac speech synthesizer delegate"));
		SpeechSynthesizer.delegate = SpeechSynthesizerDelegate;
	}, NSDefaultRunLoopMode, true);

	Volume = SpeechSynthesizer.volume;
	Rate = RateWPMToRate(SpeechSynthesizer.rate);
	UE_LOG(LogTextToSpeech, Verbose, TEXT("Mac TTS activated."));
}

void FMacTextToSpeech::OnDeactivated()
{
	ensureMsgf(IsActive(), TEXT("Attempting to deactivate an already deactivated TTS. FTextToSpeechBase::Deactivate() should already guard against this."));
	// deallocate all AppKit objects in main thread just in case
	MainThreadCall(^{
		checkf(SpeechSynthesizerDelegate, TEXT("Deactivating Mac TTS with null speech synthesizer delegate. Speech synthesizer delegate must be valid throughout the lifetime of the object."));
		[SpeechSynthesizerDelegate release];
		checkf(SpeechSynthesizer, TEXT("Deactivating Mac TTS with null speech synthesizer. Speech synthesizer must be valid throughout the lifetime of the object."));
		[SpeechSynthesizer release];
	}, NSDefaultRunLoopMode, true);
	UE_LOG(LogTextToSpeech, Verbose, TEXT("Mac TTS deactivated."));
}

#endif
