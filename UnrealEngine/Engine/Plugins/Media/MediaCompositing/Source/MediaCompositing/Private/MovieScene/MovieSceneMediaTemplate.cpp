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

/** Base struct for exectution tokens. */
struct FMediaSectionBaseExecutionToken
	: IMovieSceneExecutionToken
{
	FMediaSectionBaseExecutionToken(UMediaSource* InMediaSource, const FMovieSceneObjectBindingID& InMediaSourceProxy, int32 InMediaSourceProxyIndex)
		: BaseMediaSource(InMediaSource)
		, MediaSourceProxy(InMediaSourceProxy)
		, MediaSourceProxyIndex(InMediaSourceProxyIndex)
	{
	}

	/**
	 * Gets the media source from either the proxy binding or the media source.
	 */
	UMediaSource* GetMediaSource(IMovieScenePlayer& Player, FMovieSceneSequenceID SequenceID)
	{
		return UMovieSceneMediaSection::GetMediaSourceOrProxy(Player, SequenceID,
			BaseMediaSource, MediaSourceProxy, MediaSourceProxyIndex);
	}

	/**
	 * Returns the index to identify the media source we are using in the proxy.
	 */
	int32 GetMediaSourceProxyIndex() const { return MediaSourceProxyIndex; }

	/**
	 * Tests if we have a media source proxy.
	 */
	bool IsMediaSourceProxyValid() const { return MediaSourceProxy.IsValid(); }

private:
	UMediaSource* BaseMediaSource;
	FMovieSceneObjectBindingID MediaSourceProxy;
	int32 MediaSourceProxyIndex = 0;
};

struct FMediaSectionPreRollExecutionToken
	: FMediaSectionBaseExecutionToken
{
	FMediaSectionPreRollExecutionToken(UMediaSource* InMediaSource, FMovieSceneObjectBindingID InMediaSourceProxy, int32 InMediaSourceProxyIndex, FTimespan InStartTimeSeconds)
		: FMediaSectionBaseExecutionToken(InMediaSource, InMediaSourceProxy, InMediaSourceProxyIndex)
		, StartTime(InStartTimeSeconds)
	{ }

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		FMovieSceneMediaData& SectionData = PersistentData.GetSectionData<FMovieSceneMediaData>();
		UMediaPlayer* MediaPlayer = SectionData.GetMediaPlayer();
		IMediaPlayerProxyInterface* PlayerProxyInterface = Cast<IMediaPlayerProxyInterface>(SectionData.GetPlayerProxy());
		UMediaSource* MediaSource = GetMediaSource(Player, Operand.SequenceID);

		if (MediaPlayer == nullptr || MediaSource == nullptr)
		{
			return;
		}

		// open the media source if necessary
		if (MediaPlayer->GetUrl().IsEmpty())
		{
			FMediaPlayerOptions Options;
			Options.SetAllAsOptional();

			if (PlayerProxyInterface != nullptr)
			{
				MediaSource->SetCacheSettings(PlayerProxyInterface->GetCacheSettings());
			}
			SectionData.SeekOnOpen(StartTime);

			Options.InternalCustomOptions.Emplace(MediaPlayerOptionValues::Environment(), MediaPlayerOptionValues::Environment_Sequencer());
			MediaPlayer->OpenSourceWithOptions(MediaSource, Options);
			return;
		}

		bool bMoveToNewTime = Context.GetStatus() != EMovieScenePlayerStatus::Playing || (Context.GetStatus() == EMovieScenePlayerStatus::Playing && Context.HasJumped());
		if (bMoveToNewTime)
		{
			MediaPlayer->SetRate(0.0f);
			MediaPlayer->Seek(StartTime);
			MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>::Empty());
		}
	}

private:

	FTimespan StartTime;
};


struct FMediaSectionExecutionToken
	: FMediaSectionBaseExecutionToken
{
	FMediaSectionExecutionToken(UMediaSource* InMediaSource, FMovieSceneObjectBindingID InMediaSourceProxy, int32 InMediaSourceProxyIndex, float InProxyTextureBlend, bool bInCanPlayerBeOpen, FTimespan InCurrentTime, FTimespan InFrameDuration)
		: FMediaSectionBaseExecutionToken(InMediaSource, InMediaSourceProxy, InMediaSourceProxyIndex)
		, CurrentTime(InCurrentTime)
		, FrameDuration(InFrameDuration)
		, PlaybackRate(1.0f)
		, ProxyTextureBlend(InProxyTextureBlend)
		, bCanPlayerBeOpen(bInCanPlayerBeOpen)
	{ }

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FMovieSceneMediaData& SectionData = PersistentData.GetSectionData<FMovieSceneMediaData>();
		UMediaPlayer* MediaPlayer = SectionData.GetMediaPlayer();
		UObject* PlayerProxy = SectionData.GetPlayerProxy();
		UMediaSource* MediaSource = GetMediaSource(Player, Operand.SequenceID);

		if (MediaPlayer == nullptr || MediaSource == nullptr)
		{
			return;
		}

		// Do we have a player proxy?
		IMediaPlayerProxyInterface* PlayerProxyInterface = Cast<IMediaPlayerProxyInterface>(PlayerProxy);
		if (PlayerProxyInterface != nullptr)
		{
			PlayerProxyInterface->ProxySetTextureBlend(SectionData.GetProxyLayerIndex(), SectionData.GetProxyTextureIndex(), ProxyTextureBlend);
			// Can we control the player?
			if (PlayerProxyInterface->IsExternalControlAllowed() == false)
			{
				return;
			}

			if (SectionData.bIsAspectRatioSet == false)
			{
				if (PlayerProxyInterface->ProxySetAspectRatio(MediaPlayer))
				{
					SectionData.bIsAspectRatioSet = true;
				}
			}
		}

		// Can we be open?
		if (bCanPlayerBeOpen == false)
		{
			if (MediaPlayer->IsClosed() == false)
			{
				MediaPlayer->Close();
			}
			return;
		}

		bool bCacheSettingsChanged = false;
		FMediaSourceCacheSettings CurrentCacheSettings;
		if (PlayerProxyInterface != nullptr && MediaSource->GetCacheSettings(CurrentCacheSettings))
		{
			bCacheSettingsChanged = (CurrentCacheSettings != PlayerProxyInterface->GetCacheSettings());
		}

		// open the media source if necessary
		if (MediaPlayer->GetUrl().IsEmpty() || bCacheSettingsChanged)
		{
			FMediaPlayerOptions Options;
			Options.SetAllAsOptional();

			if (PlayerProxyInterface != nullptr)
			{
				MediaSource->SetCacheSettings(PlayerProxyInterface->GetCacheSettings());
			}
			SectionData.SeekOnOpen(CurrentTime);

			Options.InternalCustomOptions.Emplace(MediaPlayerOptionValues::Environment(), MediaPlayerOptionValues::Environment_Sequencer());
			// Setup an initial blocking range - MediaFramework will block (even through the opening process) in its next tick...
			MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>(CurrentTime, CurrentTime + FrameDuration));
			MediaPlayer->OpenSourceWithOptions(MediaSource, Options);
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
		FTimespan MediaTime = CurrentTime;

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
				if (!MediaPlayer->SetRate((Context.GetDirection() == EPlayDirection::Forwards) ? 1.0f : -1.0f))
				{
					// Failed to set needed rate. Keep things blocked, as this means the player will still not be playing, this will
					// trigger a seek to each and every frame. A potentially very SLOW method of approximating backwards playback, but better
					// than nothing.
					// -> nothing to do
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
					if (!MediaPlayer->SetRate(1.0f))
					{
						// Failed to set needed rate. Keep things blocked, as this means the player will still be returning the old rate, we will get here repeatedly
						// and each time trigger a seek. A potentially very SLOW method of approximating backwards playback, but better
						// than nothing.
						MediaPlayer->Seek(MediaTime);
					}
				}
				else if (Context.GetDirection() == EPlayDirection::Backwards && CurrentPlayerRate > 0.0f)
				{
					if (!MediaPlayer->SetRate(-1.0f))
					{
						// Failed to set needed rate. Keep things blocked, as this means the player will still be returning the old rate, we will get here repeatedly
						// and each time trigger a seek. A potentially very SLOW method of approximating backwards playback, but better
						// than nothing.
						MediaPlayer->Seek(MediaTime);
					}
				}
			}
		}
		else
		{
			if (MediaPlayer->IsPlaying())
			{
				MediaPlayer->SetRate(0.0f);
			}

			MediaPlayer->Seek(MediaTime);
		}

		// Set blocking range / time-range to display
		// (we always use the full current time for this, any adjustments to player timestamps are done internally)
		MediaPlayer->SetBlockOnTimeRange(TRange<FTimespan>(CurrentTime, CurrentTime + FrameDuration));
	}

private:

	FTimespan CurrentTime;
	FTimespan FrameDuration;
	float PlaybackRate;
	float ProxyTextureBlend;
	bool bCanPlayerBeOpen;
};


/* FMovieSceneMediaSectionTemplate structors
 *****************************************************************************/

FMovieSceneMediaSectionTemplate::FMovieSceneMediaSectionTemplate(const UMovieSceneMediaSection& InSection, const UMovieSceneMediaTrack& InTrack)
	: MediaSection(&InSection)
{
	Params.MediaSource = InSection.GetMediaSource();
	Params.MediaSourceProxy = InSection.GetMediaSourceProxy();
	Params.MediaSourceProxyIndex = InSection.MediaSourceProxyIndex;
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
	UMediaSource* MediaSource = Params.MediaSource;
	if (((MediaSource == nullptr) && (Params.MediaSourceProxy.IsValid() == false)) || Context.IsPostRoll())
	{
		return;
	}

	// @todo: account for video time dilation if/when these are added

	if (Context.IsPreRoll())
	{
		const FFrameRate FrameRate = Context.GetFrameRate();
		const FFrameNumber StartFrame = Context.HasPreRollEndTime() ? Context.GetPreRollEndFrame() - Params.SectionStartFrame + Params.StartFrameOffset : Params.StartFrameOffset;

		const double StartFrameInSeconds = FrameRate.AsSeconds(StartFrame);
		const int64 StartTicks = static_cast<int64>(StartFrameInSeconds * ETimespan::TicksPerSecond);

		ExecutionTokens.Add(FMediaSectionPreRollExecutionToken(MediaSource, Params.MediaSourceProxy, Params.MediaSourceProxyIndex, FTimespan(StartTicks)));
	}
	else if (!Context.IsPostRoll() && (Context.GetTime().FrameNumber < Params.SectionEndFrame))
	{
		const FFrameRate FrameRate = Context.GetFrameRate();
		const FFrameTime FrameTime(Context.GetTime().FrameNumber - Params.SectionStartFrame + Params.StartFrameOffset);

		const double FrameTimeInSeconds = FrameRate.AsSeconds(FrameTime);
		const int64 FrameTicks = static_cast<int64>(FrameTimeInSeconds * ETimespan::TicksPerSecond);

		// With zero-length frames (which can occur occasionally), we use the fixed frame time, matching previous behavior.
		const double FrameDurationInSeconds = FMath::Max(FrameRate.AsSeconds(FFrameTime(1)), (Context.GetRange().Size<FFrameTime>()) / Context.GetFrameRate());
		const int64 FrameDurationTicks = static_cast<int64>(FrameDurationInSeconds * ETimespan::TicksPerSecond);

		float ProxyTextureBlend = MediaSection->EvaluateEasing(Context.GetTime());
		bool bCanPlayerBeOpen = true;
		MediaSection->ChannelCanPlayerBeOpen.Evaluate(Context.GetTime(), bCanPlayerBeOpen);

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

		ExecutionTokens.Add(FMediaSectionExecutionToken(MediaSource, Params.MediaSourceProxy, Params.MediaSourceProxyIndex, ProxyTextureBlend, bCanPlayerBeOpen, FTimespan(FrameTicks), FTimespan(FrameDurationTicks)));
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
		int32 ProxyTextureIndex = 0;
		int32 ProxyLayerIndex = 0;
		if (MediaSection != nullptr)
		{
			ProxyTextureIndex = MediaSection->TextureIndex;
			ProxyLayerIndex = MediaSection->GetRowIndex();
		}

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
							MediaAssetsModule->GetPlayerFromObject(BoundObject, PlayerProxy);
							break;
						}
					}
				}
			}
		}

		// Add section data.
		SectionData = &PersistentData.AddSectionData<FMovieSceneMediaData>();
		SectionData->Setup(MediaPlayer, PlayerProxy, ProxyLayerIndex, ProxyTextureIndex);
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
	SectionData->Initialize(IsEvaluating);

	if (Params.MediaSoundComponent != nullptr)
	{
		if (IsEvaluating)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Setting media player %p on media sound component %p"), MediaPlayer, Params.MediaSoundComponent.Get());
			#endif

			Params.MediaSoundComponent->SetMediaPlayer(MediaPlayer);
		}
		else if (Params.MediaSoundComponent->GetMediaPlayer() == MediaPlayer)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Resetting media player on media sound component %p"), Params.MediaSoundComponent.Get());
			#endif

			Params.MediaSoundComponent->SetMediaPlayer(nullptr);
		}
	}

	if (Params.MediaTexture != nullptr)
	{
		if (IsEvaluating)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Setting media player %p on media texture %p"), MediaPlayer, Params.MediaTexture.Get());
			#endif

			Params.MediaTexture->SetMediaPlayer(MediaPlayer);
		}
		else if (Params.MediaTexture->GetMediaPlayer() == MediaPlayer)
		{
			#if MOVIESCENEMEDIATEMPLATE_TRACE_EVALUATION
				GLog->Logf(ELogVerbosity::Log, TEXT("Resetting media player on media texture %p"), Params.MediaTexture.Get());
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

	UObject* PlayerProxy = SectionData->GetPlayerProxy();
	if (PlayerProxy != nullptr)
	{
		IMediaPlayerProxyInterface* PlayerProxyInterface = Cast<IMediaPlayerProxyInterface>(PlayerProxy);
		if (PlayerProxyInterface != nullptr)
		{
			PlayerProxyInterface->ProxySetTextureBlend(SectionData->GetProxyLayerIndex(), SectionData->GetProxyTextureIndex(), 0.0f);
		}
	}

	SectionData->TearDown();
}

