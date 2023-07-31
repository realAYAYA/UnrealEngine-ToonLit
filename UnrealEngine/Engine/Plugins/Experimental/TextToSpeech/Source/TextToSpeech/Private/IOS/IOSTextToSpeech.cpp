// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_IOS
 #include "IOS/IOSTextToSpeech.h"
#include "TextToSpeechLog.h"
#include "IOS/IOSAppDelegate.h"
#include "Async/TaskGraphInterfaces.h"
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

/** Defined in FTextToSpeechBase.cpp */
extern TMap<TextToSpeechId, TWeakPtr<FTextToSpeechBase>> ActiveTextToSpeechMap;

@interface FSpeechSynthesizerDelegate : FApplePlatformObject<AVSpeechSynthesizerDelegate>
-(id)initWithOwningTextToSpeechId:(TextToSpeechId)InOwningTextToSpeechId;
-(void)dealloc;
-(void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance *)utterance;

@end

@implementation FSpeechSynthesizerDelegate
{
	TextToSpeechId _OwningTextToSpeechId;
}
APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FSpeechSynthesizerDelegate)
-(id)initWithOwningTextToSpeechId:(TextToSpeechId)InOwningTextToSpeechId
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

-(void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance*)utterance
{
	if (_OwningTextToSpeechId != FTextToSpeechBase::InvalidTextToSpeechId)
	{
		const TextToSpeechId IdCopy = _OwningTextToSpeechId;
		const TMap<TextToSpeechId, TWeakPtr<FTextToSpeechBase>>& ActiveTextToSpeechMapRef = ActiveTextToSpeechMap;
		FFunctionGraphTask::CreateAndDispatchWhenReady([IdCopy, ActiveTextToSpeechMapRef]()
			{
				const TWeakPtr<FTextToSpeechBase>* TTSPtr = ActiveTextToSpeechMapRef.Find(IdCopy);
				if (TTSPtr && TTSPtr->IsValid())
				{
					TTSPtr->Pin()->OnTextToSpeechFinishSpeaking_GameThread();
				}
			}, TStatId(), NULL, ENamedThreads::GameThread);
	}
}
@end

FIOSTextToSpeech::FIOSTextToSpeech()
	: bIsSpeaking(false)
	, Volume(0.0f)
	, Rate(0.0f)
	, SpeechSynthesizer(nullptr)
	, SpeechSynthesizerDelegate(nullptr)
{

}

FIOSTextToSpeech::~FIOSTextToSpeech()
{

}

void FIOSTextToSpeech::Speak(const FString& InStringToSpeak)
{
	if (IsActive())
	{
		if (!InStringToSpeak.IsEmpty())
		{
			if (IsSpeaking())
			{
				StopSpeaking();
			}
			SCOPED_AUTORELEASE_POOL;
			NSString* Announcement = InStringToSpeak.GetNSString();
			ensureMsgf(Announcement, TEXT("Failed to convert FString to NSString for TTS."));
			AVSpeechUtterance* Utterance = [AVSpeechUtterance speechUtteranceWithString:Announcement];
			checkf(Utterance, TEXT("Failed to create an utterance for text to speech."));
			// for now we just use the default system language that's being used
			Utterance.voice = [AVSpeechSynthesisVoice voiceWithLanguage: [AVSpeechSynthesisVoice currentLanguageCode]];
			// If muted, set to volume to 0
			Utterance.volume = IsMuted() ? 0.0f : Volume;
			Utterance.rate = Rate;
			[SpeechSynthesizer speakUtterance:Utterance];
			bIsSpeaking = true;
		}
	}
}

bool FIOSTextToSpeech::IsSpeaking() const
{
	return IsActive() ? bIsSpeaking : false;
}

void FIOSTextToSpeech::StopSpeaking()
{
	if (IsActive())
	{
		if (IsSpeaking())
		{
			[SpeechSynthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
		}
		bIsSpeaking = false;
	}
}

float FIOSTextToSpeech::GetVolume() const
{
	return Volume;
}

void FIOSTextToSpeech::SetVolume(float InVolume)
{
	Volume = FMath::Clamp(InVolume, 0.0f, 1.0f);
}

float FIOSTextToSpeech::GetRate() const
{
	return Rate;
}

void FIOSTextToSpeech::SetRate(float InRate)
{
	Rate = FMath::Clamp(InRate, 0.0f, 1.0f);
}

void FIOSTextToSpeech::Mute()
{
	if (!IsMuted())
	{
		SetMuted(true);
	}
}

void FIOSTextToSpeech::Unmute()
{
	if (IsMuted())
	{
		SetMuted(false);
	}
}

void FIOSTextToSpeech::OnActivated()
{
	ensureMsgf(!IsActive(), TEXT("Attempting to activate an already activated TTS. FTextToSpeechBase::Activate() should already guard against this."));
	TextToSpeechId IdCopy = GetId();
	dispatch_async(dispatch_get_main_queue(), ^
	{
		SpeechSynthesizer = [[AVSpeechSynthesizer alloc] init];
		SpeechSynthesizerDelegate = [[FSpeechSynthesizerDelegate alloc] initWithOwningTextToSpeechId:IdCopy];
		SpeechSynthesizer.delegate = SpeechSynthesizerDelegate;
		// To get initial volume and rate, we need to retrieve it from an utterance
		AVSpeechUtterance* Utterance = [AVSpeechUtterance speechUtteranceWithString:@"Temp"];
		checkf(Utterance, TEXT("Failed to create an utterance for TTS."));
		Volume = Utterance.volume;
		Rate = Utterance.rate;
		ensure(Volume >= 0 && Volume <= 1.0f);
		ensure(Rate >= 0 && Rate <= 1.0f);
		// This allows us to still hear the TTS when the IOS ringer is muted
		// SetFeature already takes care of keeping track of how many requests are made to activate/deactivate an audio feature
		[[IOSAppDelegate GetDelegate] SetFeature:EAudioFeature::Playback Active:true];
	});
}

void FIOSTextToSpeech::OnDeactivated()
{
	ensureMsgf(IsActive(), TEXT("Attempting to deactivate an already deactivated TTS. FTextToSpeechBase::Deactivate() should already guard against this."));
	// deallocate all AppKit objects in main thread just in case
	dispatch_async(dispatch_get_main_queue(), ^
	{
		checkf(SpeechSynthesizerDelegate, TEXT("Null speech synthesizer in IOS tex to speech. A valid delegate must exist for the lifetime of the TTS."));
		[SpeechSynthesizerDelegate release];
		checkf(SpeechSynthesizer, TEXT("Null speech synthesizer in IOS text to speech. There should be a valid speech  synthesizer for the lifetime of the TTS."));
		[SpeechSynthesizer release];
		// SetFeature already takes care of keeping track of the number of requests to activate/deactivate a feature
		[[IOSAppDelegate GetDelegate] SetFeature:EAudioFeature::Playback Active:false];
	});
};
#endif
