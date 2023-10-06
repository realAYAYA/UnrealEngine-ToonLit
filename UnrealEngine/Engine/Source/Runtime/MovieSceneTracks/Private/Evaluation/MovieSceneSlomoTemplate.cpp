// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSlomoTemplate.h"
#include "Tracks/MovieSceneSlomoTrack.h"
#include "MovieSceneSequence.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "EngineGlobals.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSlomoTemplate)


DECLARE_CYCLE_STAT(TEXT("Slomo Track Token Execute"), MovieSceneEval_SlomoTrack_TokenExecute, STATGROUP_MovieSceneEval);


struct FSlomoTrackToken
{
	FSlomoTrackToken(float InSlomoValue)
		: SlomoValue(InSlomoValue)
	{}

	float SlomoValue;

	void Apply(IMovieScenePlayer& Player)
	{
		UObject* PlaybackContext = Player.GetPlaybackContext();
		UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

		if (!World || (!GIsEditor && World->GetNetMode() == NM_Client) || SlomoValue <= 0.f)
		{
			return;
		}

		AWorldSettings* WorldSettings = World->GetWorldSettings();

		if (WorldSettings)
		{
			WorldSettings->CinematicTimeDilation = SlomoValue;
			WorldSettings->ForceNetUpdate();
		}
	}
};

struct FSlomoTrackData : IPersistentEvaluationData
{
	TOptional<FSlomoTrackToken> PreviousSlomoValue;
};

struct FSlomoPreAnimatedGlobalToken : FSlomoTrackToken, IMovieScenePreAnimatedGlobalToken
{
	FSlomoPreAnimatedGlobalToken(float InSlomoValue)
		: FSlomoTrackToken(InSlomoValue)
	{}

	virtual void RestoreState(const UE::MovieScene::FRestoreStateParams& Params) override
	{
		IMovieScenePlayer* Player = Params.GetTerminalPlayer();
		if (!ensure(Player))
		{
			return;
		}

		Apply(*Player);
	}
};

struct FSlomoPreAnimatedGlobalTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
{
	FSlomoPreAnimatedGlobalTokenProducer(IMovieScenePlayer& InPlayer) 
		: Player(InPlayer)
	{}

	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
	{
		if (AWorldSettings* WorldSettings = Player.GetPlaybackContext()->GetWorld()->GetWorldSettings())
		{
			return FSlomoPreAnimatedGlobalToken(WorldSettings->CinematicTimeDilation);
		}
		return IMovieScenePreAnimatedGlobalTokenPtr();
	}

	IMovieScenePlayer& Player;
};

/** A movie scene execution token that applies slomo */
struct FSlomoExecutionToken : IMovieSceneExecutionToken, FSlomoTrackToken
{
	FSlomoExecutionToken(float InSlomoValue)
		: FSlomoTrackToken(InSlomoValue)
	{}

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FSlomoExecutionToken>();
	}
	
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SlomoTrack_TokenExecute)

		Player.SavePreAnimatedState(GetAnimTypeID(), FSlomoPreAnimatedGlobalTokenProducer(Player));
		
		Apply(Player);
	}
};

FMovieSceneSlomoSectionTemplate::FMovieSceneSlomoSectionTemplate(const UMovieSceneSlomoSection& Section)
	: SlomoCurve(Section.FloatCurve)
{
}

void FMovieSceneSlomoSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	float SlomoValue = 1.f;
	if (SlomoCurve.Evaluate(Context.GetTime(), SlomoValue) && SlomoValue >= 0.f)
	{
		ExecutionTokens.Add(FSlomoExecutionToken(SlomoValue));
	}
}

