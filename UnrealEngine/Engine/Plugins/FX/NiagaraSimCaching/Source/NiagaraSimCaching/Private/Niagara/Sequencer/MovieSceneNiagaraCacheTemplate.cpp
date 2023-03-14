// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraCacheTemplate.h"

#include "NiagaraComponent.h"

DECLARE_CYCLE_STAT(TEXT("Niagara Cache - Token Execute"), MovieSceneEval_NiagaraCache_TokenExecute, STATGROUP_MovieSceneEval);

/** Used to set Manual Tick back to previous when outside section */
struct FPreAnimatedNiagaraCacheTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			explicit FToken(const UNiagaraComponent* InNiagaraComponent)
			{
				AgeMode = InNiagaraComponent->GetAgeUpdateMode();
			}

			virtual void RestoreState(UObject& ObjectToRestore, const UE::MovieScene::FRestoreStateParams& Params) override
			{
				UNiagaraComponent* NiagaraComponent = CastChecked<UNiagaraComponent>(&ObjectToRestore);
				NiagaraComponent->SetAgeUpdateMode(AgeMode);
				NiagaraComponent->SetSimCache(nullptr);
			}

			ENiagaraAgeUpdateMode AgeMode;
		};

		return FToken(CastChecked<UNiagaraComponent>(&Object));
	}
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FPreAnimatedNiagaraCacheTokenProducer>();
	}
};

/** A movie scene execution token that executes a NiagaraCache */
struct FNiagaraCacheExecutionToken : IMovieSceneExecutionToken
{
	FNiagaraCacheExecutionToken(const FMovieSceneNiagaraCacheSectionTemplateParameters &InParams) :
		Params(InParams)
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_NiagaraCache_TokenExecute)
		if (Operand.ObjectBindingID.IsValid() && Params.NiagaraCacheParams.SimCache && Params.NiagaraCacheParams.SimCache->GetNumFrames() > 0)
		{
			for (TWeakObjectPtr<> WeakObj : Player.FindBoundObjects(Operand))
			{
				if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(WeakObj))
				{
					if (NiagaraComponent->GetSimCache() != Params.NiagaraCacheParams.SimCache)
					{
						NiagaraComponent->SetSimCache(Params.NiagaraCacheParams.SimCache);
					}
					
					// calculate the time at which to evaluate the sim cache
					float Age = Params.MapTimeToAnimation(Params.NiagaraCacheParams, Params.NiagaraCacheParams.SimCache->GetDurationSeconds(), Context.GetTime(), Context.GetFrameRate()) + Params.NiagaraCacheParams.SimCache->GetStartSeconds();

					Player.SavePreAnimatedState(*NiagaraComponent, FPreAnimatedNiagaraCacheTokenProducer::GetAnimTypeID(), FPreAnimatedNiagaraCacheTokenProducer());
					NiagaraComponent->SetDesiredAge(Age);
					NiagaraComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
					if (!NiagaraComponent->IsActive())
					{
						NiagaraComponent->ResetSystem();
					}
				}
			}
		}
	}

	FMovieSceneNiagaraCacheSectionTemplateParameters Params;
};

FMovieSceneNiagaraCacheSectionTemplate::FMovieSceneNiagaraCacheSectionTemplate(const UMovieSceneNiagaraCacheSection& InSection)
	: Params(const_cast<FMovieSceneNiagaraCacheParams&> (InSection.Params), InSection.GetInclusiveStartFrame(), InSection.GetExclusiveEndFrame())
{
}

//We use a token here so we can set the manual tick state back to what it was previously when outside this section.
//This is similar to how Skeletal Animation evaluation also works.
void FMovieSceneNiagaraCacheSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.Add(FNiagaraCacheExecutionToken(Params));
}