// Copyright Epic Games, Inc. All Rights Reserved.

#include "LinearTimecodeComponent.h"
#include "LinearTimecodeDecoder.h"

#include "IMediaAudioSample.h"
#include "IMediaPlayer.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"

#include "MediaAudioSampleReader.h"

void ULinearTimecodeComponent::ProcessAudio(TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> InSampleQueue)
{
	DropTimecode.bNewFrame = 0;
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> AudioSample;

	while (SampleQueue->Dequeue(AudioSample))
	{
		check(AudioSample.IsValid());

		MediaAudioSampleReader SampleReader(AudioSample);
		float Sample;
		while (SampleReader.GetSample(Sample))
		{
			if (TimecodeDecoder->Sample(Sample, DropTimecode))
			{
				DropTimecode.bNewFrame = true;
				OnTimecodeChange.Broadcast(DropTimecode);
			}
		}
	}
}

/* ULinearTimecodeComponent 
*****************************************************************************/

ULinearTimecodeComponent::ULinearTimecodeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimecodeDecoder(MakeShared<class FLinearTimecodeDecoder, ESPMode::ThreadSafe>())
{
	PrimaryComponentTick.bCanEverTick = true;
}

/* UMediaSoundComponent interface
*****************************************************************************/

void ULinearTimecodeComponent::UpdatePlayer()
{
	if (MediaPlayer == nullptr)
	{
		SampleQueue.Reset();
		return;
	}

	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacade = MediaPlayer->GetPlayerFacade();

	if (PlayerFacade != CurrentPlayerFacade)
	{
		// Create a new sample queue if the player changed
		// Sinks are weak, so don't need to be released
		SampleQueue = MakeShared<FMediaAudioSampleQueue, ESPMode::ThreadSafe>();
		PlayerFacade->AddAudioSampleSink(SampleQueue.ToSharedRef());
		CurrentPlayerFacade = PlayerFacade;
	}

	// Process the audio, looking for timecode frames
	if (SampleQueue.IsValid())
	{
		ProcessAudio(SampleQueue);
	}
}

/* UActorComponent interface
*****************************************************************************/

void ULinearTimecodeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePlayer();
}

/* USceneComponent interface
*****************************************************************************/

void ULinearTimecodeComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate())
	{
		SetComponentTickEnabled(true);
	}

	Super::Activate(bReset);
}

void ULinearTimecodeComponent::Deactivate()
{
	if (!ShouldActivate())
	{
		SetComponentTickEnabled(false);
	}

	Super::Deactivate();
}

void ULinearTimecodeComponent::GetDropTimeCodeFrameNumber(const FDropTimecode& Timecode, int32& FrameNumber)
{
	int32 NumMinutes = Timecode.Timecode.Hours * 60 + Timecode.Timecode.Minutes;
	int32 NumFrames = (Timecode.Timecode.Hours * 3600 + Timecode.Timecode.Minutes * 60 + Timecode.Timecode.Seconds) * Timecode.FrameRate + Timecode.Timecode.Frames;
	NumFrames -= (Timecode.Timecode.bDropFrameFormat) ? 2 * (NumMinutes - (NumMinutes / 10)) : 0;
	FrameNumber = NumFrames;
}

int32 ULinearTimecodeComponent::GetDropFrameNumber() const
{
	int32 FrameNumber;
	GetDropTimeCodeFrameNumber(DropTimecode, FrameNumber);
	return FrameNumber;
}

double ULinearTimecodeComponent::FrameRateToFrameDelta(int32 InFrameRate, int32 InDrop)
{
	return static_cast<double>(InFrameRate * 1000) / (InDrop ? 1001 : 1000);
}

void ULinearTimecodeComponent::SetDropTimecodeFrameNumber(const FDropTimecode& Timecode, int32 InFrame, FDropTimecode& OutTimecode)
{
	OutTimecode = Timecode;
	int32 FrameNum = InFrame;

	if (Timecode.Timecode.bDropFrameFormat)
	{
		double FrameRate = FrameRateToFrameDelta(Timecode.FrameRate, Timecode.Timecode.bDropFrameFormat);
		int32 TenMinutes = static_cast<int32>(60 * FrameRate * 10);
		int32 OneMinute = static_cast<int32>(60 * FrameRate);

		int32 LotsOfTenMinutes = FrameNum / TenMinutes;
		int32 RemainderOfTenMinute = FrameNum % TenMinutes;

		RemainderOfTenMinute = (RemainderOfTenMinute < 0) ? 0 : RemainderOfTenMinute;
		FrameNum += 18 * LotsOfTenMinutes + 2 * ((RemainderOfTenMinute - 2) / OneMinute);
	}

	OutTimecode.Timecode.Frames = FrameNum % Timecode.FrameRate;
	FrameNum /= Timecode.FrameRate;
	OutTimecode.Timecode.Seconds = FrameNum % 60;
	FrameNum /= 60;
	OutTimecode.Timecode.Minutes = FrameNum % 60;
	FrameNum /= 60;
	OutTimecode.Timecode.Hours = FrameNum % 24;
}

//// Added to BluePrintLibrary to allow type conversion

FString UDropTimecodeToStringConversion::Conv_DropTimecodeToString(const FDropTimecode& InTimecode)
{
	return InTimecode.Timecode.ToString(false);
}

