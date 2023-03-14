// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaTemplate.h"

#include "IMediaAssetsModule.h"
#include "Math/UnrealMathUtility.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaPlayerProxyInterface.h"
#include "MediaSoundComponent.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "UObject/Package.h"
#include "UObject/GCObject.h"

#include "MovieSceneMediaData.h"
#include "MovieSceneMediaSection.h"
#include "MovieSceneMediaTrack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneMediaTemplate)


#define MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION 0


/* Local helpers
 *****************************************************************************/

struct FMediaSectionPreRollExecutionToken
	: IMovieSceneExecutionToken
{
	FMediaSectionPreRollExecutionToken(UMediaSource* InMediaSource, FTimespan InStartTimeSeconds)
		: MediaSource(InMediaSource)
		, StartTime(InStartTimeSeconds)
	{ }

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		FMovieSceneMediaData& SectionData = PersistentData.GetSectionData<FMovieSceneMediaData>();
		UMediaPlayer* MediaPlayer = SectionData.GetMediaPlayer();
		UObject* PlayerProxy = SectionData.GetPlayerProxy();

		if (MediaPlayer == nullptr || MediaSource == nullptr)
		{
			return;
		}

		// Do we have a player proxy?
		IMediaPlayerProxyInterface* PlayerProxyInterface = nullptr;
		if (PlayerProxy != nullptr)
		{
			PlayerProxyInterface = Cast<IMediaPlayerProxyInterface>
				(PlayerProxy);
		}

		// open the media source if necessary
		if (MediaPlayer->GetUrl().IsEmpty())
		{
			if (PlayerProxyInterface != nullptr)
			{
				MediaSource->SetCacheSettings(PlayerProxyInterface->GetCacheSettings());
			}
			SectionData.SeekOnOpen(StartTime);
			MediaPlayer->OpenSource(MediaSource);
		}
	}

private:

	UMediaSource* MediaSource;
	FTimespan StartTime;
};


struct FMediaSectionExecutionToken
	: IMovieSceneExecutionToken
{
	FMediaSectionExecutionToken(UMediaSource* InMediaSource, FTimespan InCurrentTime, FTimespan InFrameDuration)
		: CurrentTime(InCurrentTime)
		, FrameDuration(InFrameDuration)
		, MediaSource(InMediaSource)
		, PlaybackRate(1.0f)
	{ }

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FMovieSceneMediaData& SectionData = PersistentData.GetSectionData<FMovieSceneMediaData>();
		UMediaPlayer* MediaPlayer = SectionData.GetMediaPlayer();
		UObject* PlayerProxy = SectionData.GetPlayerProxy();

		if (MediaPlayer == nullptr || MediaSource == nullptr)
		{
			return;
		}

		// Do we have a player proxy?
		IMediaPlayerProxyInterface* PlayerProxyInterface = nullptr;
		if (PlayerProxy != nullptr)
		{
			PlayerProxyInterface = Cast<IMediaPlayerProxyInterface>
				(PlayerProxy);
			if (PlayerProxyInterface != nullptr)
			{
				// Can we control the player?
				if (PlayerProxyInterface->IsExternalControlAllowed() == false)
				{
					return;
				}
			}
		}

		// open the media source if necessary
		if (MediaPlayer->GetUrl().IsEmpty())
		{
			if (PlayerProxyInterface != nullptr)
			{
				MediaSource->SetCacheSettings(PlayerProxyInterface->GetCacheSettings());
			}
			SectionData.SeekOnOpen(CurrentTime);
			// Setup an initial blocking range - MediaFramework will block (even through the opening process) in its next tick...
			MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>(CurrentTime, CurrentTime + FrameDuration));
			MediaPlayer->OpenSource(MediaSource);

			return;
		}

		// seek on open if necessary
		// (usually should not be needed as the blocking on open should ensure we never see the player preparing here)
		if (MediaPlayer->IsPreparing())
		{
			SectionData.SeekOnOpen(CurrentTime);
			MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>(CurrentTime, CurrentTime + FrameDuration));

			return;
		}

		const FTimespan MediaDuration = MediaPlayer->GetDuration();

		if (MediaDuration.IsZero())
		{
			return; // media has no length
		}

		//
		// update media player
		//

		// Setup media time (used for seeks)
		FTimespan MediaTime;
		if (!MediaPlayer->IsLooping())
		{
			// note: we use a small offset at the end to make sure we can indeed seek to it (exclusive end type range)
			MediaTime = FMath::Clamp(CurrentTime, FTimespan::Zero(), MediaDuration - FrameDuration * 0.5);
		}
		else
		{
			// one always seeks into the original media time-range, hence: modulo the time
			MediaTime = CurrentTime % MediaDuration;
		}

		#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
			GLog->Logf(ELogVerbosity::Log, TEXT("Executing time %s, MediaTime %s"), *CurrentTime.ToString(TEXT("%h:%m:%s.%t")), *MediaTime.ToString(TEXT("%h:%m:%s.%t")));
		#endif

		if (Context.GetStatus() == EMovieScenePlayerStatus::Playing)
		{
			if (!MediaPlayer->IsPlaying())
			{
				MediaPlayer->Seek(MediaTime);
				// Set rate
				// (note that the DIRECTION is important, but the magnitude is not - as we use blocked playback, the range setup to block on will serve as external clock to the player,
				//  the direction is taken into account as hint for internal operation of the player)
				if (!SetRate(MediaPlayer, PlayerProxyInterface,
					(Context.GetDirection() == EPlayDirection::Forwards) ? 1.0f : -1.0f))
				{
					// Failed to set needed rate: better switch off blocking and bail...
					MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>::Empty());
					return;
				}
			}
			else
			{
				if (Context.HasJumped())
				{
					MediaPlayer->Seek(MediaTime);
				}

				float CurrentPlayerRate = MediaPlayer->GetRate();
				if (Context.GetDirection() == EPlayDirection::Forwards && CurrentPlayerRate < 0.0f)
				{
					if (!SetRate(MediaPlayer, PlayerProxyInterface, 1.0f))
					{
						// Failed to set needed rate: better switch off blocking and bail...
						MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>::Empty());
						return;
					}
				}
				else if (Context.GetDirection() == EPlayDirection::Backwards && CurrentPlayerRate > 0.0f)
				{
					if (!SetRate(MediaPlayer, PlayerProxyInterface, -1.0f))
					{
						// Failed to set needed rate: better switch off blocking and bail...
						MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>::Empty());
						return;
					}
				}
			}
		}
		else
		{
			if (MediaPlayer->IsPlaying())
			{
				SetRate(MediaPlayer, PlayerProxyInterface, 0.0f);
			}

			MediaPlayer->Seek(MediaTime);
		}

	// Set blocking range / time-range to display
	// (we always use the full current time for this, any adjustments to player timestamps are done internally)
		MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>(CurrentTime, CurrentTime + FrameDuration));
	}

private:

	/**
	 * Call this to set the rate.
	 * It will use the proxy if available, otherwise it will use the player.
	 */
	bool SetRate(UMediaPlayer* Player, IMediaPlayerProxyInterface* Proxy, float Rate)
	{
		if (Proxy != nullptr)
		{
			return Proxy->SetProxyRate(Rate);
		}
		else
		{
			return Player->SetRate(Rate);
		}
	}

	FTimespan CurrentTime;
	FTimespan FrameDuration;
	UMediaSource* MediaSource;
	float PlaybackRate;
};


/* FMovieSceneMediaSectionTemplate structors
 *****************************************************************************/

FMovieSceneMediaSectionTemplate::FMovieSceneMediaSectionTemplate(const UMovieSceneMediaSection& InSection, const UMovieSceneMediaTrack& InTrack)
{
	Params.MediaSource = InSection.GetMediaSource();
	Params.MediaSoundComponent = InSection.MediaSoundComponent;
	Params.bLooping = InSection.bLooping;
	Params.StartFrameOffset = InSection.StartFrameOffset;
	if (Params.MediaSource != nullptr)
	{
		Params.MediaSource->SetCacheSettings(InSection.CacheSettings);
	}

	// If using an external media player link it here so we don't automatically create it later.
	Params.MediaPlayer = InSection.bUseExternalMediaPlayer ? InSection.ExternalMediaPlayer : nullptr;
	Params.MediaTexture = InSection.bUseExternalMediaPlayer ? nullptr : InSection.MediaTexture;

	if (InSection.HasStartFrame())
	{
		Params.SectionStartFrame = InSection.GetRange().GetLowerBoundValue();
	}
	if (InSection.HasEndFrame())
	{
		Params.SectionEndFrame = InSection.GetRange().GetUpperBoundValue();
	}
}


/* FMovieSceneEvalTemplate interface
 *****************************************************************************/

void FMovieSceneMediaSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if ((Params.MediaSource == nullptr) || Context.IsPostRoll())
	{
		return;
	}

	// @todo: account for video time dilation if/when these are added

	if (Context.IsPreRoll())
	{
		const FFrameRate FrameRate = Context.GetFrameRate();
		const FFrameNumber StartFrame = Context.HasPreRollEndTime() ? Context.GetPreRollEndFrame() - Params.SectionStartFrame + Params.StartFrameOffset : Params.StartFrameOffset;

		const double StartFrameInSeconds = FrameRate.AsSeconds(StartFrame);
		const int64 StartTicks = StartFrameInSeconds * ETimespan::TicksPerSecond;

		ExecutionTokens.Add(FMediaSectionPreRollExecutionToken(Params.MediaSource, FTimespan(StartTicks)));
	}
	else if (!Context.IsPostRoll() && (Context.GetTime().FrameNumber < Params.SectionEndFrame))
	{
		const FFrameRate FrameRate = Context.GetFrameRate();
		const FFrameTime FrameTime(Context.GetTime().FrameNumber - Params.SectionStartFrame + Params.StartFrameOffset);

		const double FrameTimeInSeconds = FrameRate.AsSeconds(FrameTime);
		const int64 FrameTicks = FrameTimeInSeconds * ETimespan::TicksPerSecond;

		// With zero-length frames (which can occur occasionally), we use the fixed frame time, matching previous behavior.
		const double FrameDurationInSeconds = FMath::Max(FrameRate.AsSeconds(FFrameTime(1)), (Context.GetRange().Size<FFrameTime>()) / Context.GetFrameRate());
		const int64 FrameDurationTicks = FrameDurationInSeconds * ETimespan::TicksPerSecond;

		#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
			GLog->Logf(ELogVerbosity::Log, TEXT("Evaluating frame %i+%f, FrameRate %i/%i, FrameTicks %d, FrameDurationTicks %d"),
				Context.GetTime().GetFrame().Value,
				Context.GetTime().GetSubFrame(),
				FrameRate.Numerator,
				FrameRate.Denominator,
				FrameTicks,
				FrameDurationTicks
			);
		#endif

		ExecutionTokens.Add(FMediaSectionExecutionToken(Params.MediaSource, FTimespan(FrameTicks), FTimespan(FrameDurationTicks)));
	}
}


UScriptStruct& FMovieSceneMediaSectionTemplate::GetScriptStructImpl() const
{
	return *StaticStruct();
}


void FMovieSceneMediaSectionTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FMovieSceneMediaData* SectionData = PersistentData.FindSectionData<FMovieSceneMediaData>();
	if (SectionData == nullptr)
	{
		// Are we overriding the media player?
		UMediaPlayer* MediaPlayer = Params.MediaPlayer;
		UObject* PlayerProxy = nullptr;
		if (MediaPlayer == nullptr)
		{
			// Nope... do we have an object binding?
			if (Operand.ObjectBindingID.IsValid())
			{
				// Yes. Get the media player from the object.
				IMediaAssetsModule* MediaAssetsModule = FModuleManager::LoadModulePtr<IMediaAssetsModule>("MediaAssets");
				if (MediaAssetsModule != nullptr)
				{
					for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
					{
						UObject* BoundObject = WeakObject.Get();
						if (BoundObject != nullptr)
						{
							MediaPlayer = MediaAssetsModule->GetPlayerFromObject(BoundObject, PlayerProxy);
							break;
						}
					}
				}
			}
		}

		// Add section data.
		SectionData = &PersistentData.AddSectionData<FMovieSceneMediaData>();
		SectionData->Setup(MediaPlayer, PlayerProxy);
	}

	if (!ensure(SectionData != nullptr))
	{
		return;
	}

	UMediaPlayer* MediaPlayer = SectionData->GetMediaPlayer();

	if (MediaPlayer == nullptr)
	{
		return;
	}

	const bool IsEvaluating = !(Context.IsPreRoll() || Context.IsPostRoll() || (Context.GetTime().FrameNumber >= Params.SectionEndFrame));

	if (Params.MediaSoundComponent != nullptr)
	{
		if (IsEvaluating)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Setting media player %p on media sound component %p"), MediaPlayer, Params.MediaSoundComponent);
			#endif

			Params.MediaSoundComponent->SetMediaPlayer(MediaPlayer);
		}
		else if (Params.MediaSoundComponent->GetMediaPlayer() == MediaPlayer)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Resetting media player on media sound component %p"), Params.MediaSoundComponent);
			#endif

			Params.MediaSoundComponent->SetMediaPlayer(nullptr);
		}
	}

	if (Params.MediaTexture != nullptr)
	{
		if (IsEvaluating)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Setting media player %p on media texture %p"), MediaPlayer, Params.MediaTexture);
			#endif

			Params.MediaTexture->SetMediaPlayer(MediaPlayer);
		}
		else if (Params.MediaTexture->GetMediaPlayer() == MediaPlayer)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Resetting media player on media texture %p"), Params.MediaTexture);
			#endif

			Params.MediaTexture->SetMediaPlayer(nullptr);
		}
	}

	MediaPlayer->SetLooping(Params.bLooping);
}


void FMovieSceneMediaSectionTemplate::SetupOverrides()
{
	EnableOverrides(RequiresInitializeFlag | RequiresTearDownFlag);
}


void FMovieSceneMediaSectionTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FMovieSceneMediaData* SectionData = PersistentData.FindSectionData<FMovieSceneMediaData>();

	if (!ensure(SectionData != nullptr))
	{
		return;
	}

	UMediaPlayer* MediaPlayer = SectionData->GetMediaPlayer();

	if (MediaPlayer == nullptr)
	{
		return;
	}

	if ((Params.MediaSoundComponent != nullptr) && (Params.MediaSoundComponent->GetMediaPlayer() == MediaPlayer))
	{
		Params.MediaSoundComponent->SetMediaPlayer(nullptr);
	}

	if ((Params.MediaTexture != nullptr) && (Params.MediaTexture->GetMediaPlayer() == MediaPlayer))
	{
		Params.MediaTexture->SetMediaPlayer(nullptr);
	}
}

