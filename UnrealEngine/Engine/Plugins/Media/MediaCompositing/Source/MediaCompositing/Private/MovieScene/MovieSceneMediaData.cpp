// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaData.h"

#include "IMediaEventSink.h"
#include "MediaPlayer.h"
#include "MediaPlayerProxyInterface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"


/* FMediaSectionData structors
 *****************************************************************************/

FMovieSceneMediaData::FMovieSceneMediaData()
	: bOverrideMediaPlayer(false)
	, MediaPlayer(nullptr)
	, SeekOnOpenTime(FTimespan::MinValue())
{ }


FMovieSceneMediaData::~FMovieSceneMediaData()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
		MediaPlayer->Close();
		MediaPlayer->RemoveFromRoot();
	}
}


/* FMediaSectionData interface
 *****************************************************************************/

UMediaPlayer* FMovieSceneMediaData::GetMediaPlayer()
{
	return MediaPlayer;
}


void FMovieSceneMediaData::SeekOnOpen(FTimespan Time)
{
	SeekOnOpenTime = Time;
}


void FMovieSceneMediaData::Setup(UMediaPlayer* OverrideMediaPlayer, UObject* InPlayerProxy)
{
	// Ensure we don't already have a media player set. Setup should only be called once
	check(!MediaPlayer);

	if (OverrideMediaPlayer)
	{
		MediaPlayer = OverrideMediaPlayer;
		bOverrideMediaPlayer = true;
	}
	else if (MediaPlayer == nullptr)
	{
		MediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass()));
	}

	MediaPlayer->PlayOnOpen = false;
	MediaPlayer->OnMediaEvent().AddRaw(this, &FMovieSceneMediaData::HandleMediaPlayerEvent);
	MediaPlayer->AddToRoot();

	// Do we have a valid proxy object?
	if ((InPlayerProxy != nullptr) && (InPlayerProxy->Implements<UMediaPlayerProxyInterface>()))
	{
		PlayerProxy = InPlayerProxy;
	}
	else
	{
		PlayerProxy.Reset();
	}
}


/* FMediaSectionData callbacks
 *****************************************************************************/

void FMovieSceneMediaData::HandleMediaPlayerEvent(EMediaEvent Event)
{
	if ((Event != EMediaEvent::MediaOpened) || (SeekOnOpenTime < FTimespan::Zero()))
	{
		return; // we only care about seek on open
	}

	if (!MediaPlayer->SupportsSeeking())
	{
		return; // media can't seek
	}

	const FTimespan Duration = MediaPlayer->GetDuration();
    if (Duration == FTimespan::Zero())
    {
	    return;
    }

	FTimespan MediaTime;
	if (!MediaPlayer->IsLooping())
	{
		MediaTime = FMath::Clamp(SeekOnOpenTime, FTimespan::Zero(), Duration - FTimespan::FromSeconds(0.001));
	}
	else
	{
		MediaTime = SeekOnOpenTime % Duration;
	}

	MediaPlayer->SetRate(0.0f);
	MediaPlayer->Seek(MediaTime);

	SeekOnOpenTime = FTimespan::MinValue();
}
