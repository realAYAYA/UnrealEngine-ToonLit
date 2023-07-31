// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneCameraShakeSourceShakeTemplate.h"
#include "Evaluation/MovieSceneCameraShakePreviewer.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeSourceComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneCameraShakeSourceShakeTemplate)

enum class ECameraShakeSourceShakeStatus : uint8
{
	NotStarted,
	Started,
	BlendingOut
};

struct FCameraShakeSourceShakeSectionInstanceData : IPersistentEvaluationData
{
	ECameraShakeSourceShakeStatus Status = ECameraShakeSourceShakeStatus::NotStarted;
	TOptional<FFrameTime> Duration;
	TOptional<FFrameTime> BlendOutTime;

#if WITH_EDITOR
	FCameraShakePreviewer Previewer;
#endif
};

struct FPreAnimatedCameraShakeSourceShakeTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
{
	FMovieSceneEvaluationOperand Operand;

	FPreAnimatedCameraShakeSourceShakeTokenProducer(FMovieSceneEvaluationOperand InOperand)
		: Operand(InOperand)
	{
	}

	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
	{
		struct FRestoreToken : IMovieScenePreAnimatedGlobalToken
		{
			FMovieSceneEvaluationOperand Operand;

			FRestoreToken(FMovieSceneEvaluationOperand InOperand) : Operand(InOperand) {}

			virtual void RestoreState(const UE::MovieScene::FRestoreStateParams& Params) override
			{
				IMovieScenePlayer* Player = Params.GetTerminalPlayer();
				if (!ensure(Player))
				{
					return;
				}

				for (TWeakObjectPtr<> BoundObject : Player->FindBoundObjects(Operand))
				{
					if (BoundObject.IsValid())
					{
						if (UCameraShakeSourceComponent* ShakeSourceComponent = CastChecked<UCameraShakeSourceComponent>(BoundObject.Get()))
						{
							ShakeSourceComponent->StopAllCameraShakes();
						}
					}
				}
			}
		};

		return FRestoreToken(Operand);
	}
};

struct FCameraShakeSourceShakeStartExecutionToken : IMovieSceneExecutionToken
{
	const FMovieSceneCameraShakeSectionData& SourceData;

	FCameraShakeSourceShakeStartExecutionToken(const FMovieSceneCameraShakeSectionData& InSourceData)
		: SourceData(InSourceData)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FCameraShakeSourceShakeSectionInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceShakeSectionInstanceData>();

		for (TWeakObjectPtr<> BoundObject : Player.FindBoundObjects(Operand))
		{
			if (BoundObject.IsValid())
			{
				if (UCameraShakeSourceComponent* ShakeSourceComponent = CastChecked<UCameraShakeSourceComponent>(BoundObject.Get()))
				{
					static FMovieSceneAnimTypeID ShakeTypeID = TMovieSceneAnimTypeID<FCameraShakeSourceShakeStartExecutionToken, 0>();
					Player.SavePreAnimatedState(ShakeTypeID, FPreAnimatedCameraShakeSourceShakeTokenProducer(Operand));

					TSubclassOf<UCameraShakeBase> ShakeClass = SourceData.ShakeClass;
					if (ShakeClass.Get() == nullptr)
					{
						ShakeClass = ShakeSourceComponent->CameraShake;
					}

					if (ShakeClass.Get() != nullptr)
					{
						// Get the duration of the shake and store it in the instance data.
						FCameraShakeDuration ShakeDuration;
						UCameraShakeBase::GetCameraShakeDuration(ShakeClass, ShakeDuration);
						if (ShakeDuration.IsFixed())
						{
							InstanceData.Duration = Context.GetFrameRate().AsFrameTime(ShakeDuration.Get());
						}

						// Get the blend out duration and also store it in the instance data.
						float BlendIn = 0.f, BlendOut = 0.f;
						UCameraShakeBase::GetCameraShakeBlendTimes(ShakeClass, BlendIn, BlendOut);
						if (BlendOut > SMALL_NUMBER)
						{
							InstanceData.BlendOutTime = Context.GetFrameRate().AsFrameTime(BlendOut);
						}

						// Start playing the shake.
						ShakeSourceComponent->StartCameraShake(ShakeClass, SourceData.PlayScale, SourceData.PlaySpace, SourceData.UserDefinedPlaySpace);

#if WITH_EDITOR
						// Also start playing the shake in our editor preview.
						UCameraModifier_CameraShake* const PreviewCameraShake = InstanceData.Previewer.GetCameraShake();

						FAddCameraShakeParams Params;
						Params.SourceComponent = ShakeSourceComponent;
						Params.Scale = SourceData.PlayScale;
						Params.PlaySpace = SourceData.PlaySpace;
						Params.UserPlaySpaceRot = SourceData.UserDefinedPlaySpace;
						PreviewCameraShake->AddCameraShake(ShakeClass, Params);
#endif
					}
				}
			}

			// TODO-ludovic: only support a single binding for now.
			break;
		}
	}
};

struct FCameraShakeSourceShakeBlendOutExecutionToken : IMovieSceneExecutionToken
{
	const FMovieSceneCameraShakeSectionData& SourceData;

	FCameraShakeSourceShakeBlendOutExecutionToken(const FMovieSceneCameraShakeSectionData& InSourceData)
		: SourceData(InSourceData)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FCameraShakeSourceShakeSectionInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceShakeSectionInstanceData>();

		for (TWeakObjectPtr<> BoundObject : Player.FindBoundObjects(Operand))
		{
			if (BoundObject.IsValid())
			{
				if (UCameraShakeSourceComponent* ShakeSourceComponent = CastChecked<UCameraShakeSourceComponent>(BoundObject.Get()))
				{
					TSubclassOf<UCameraShakeBase> ShakeClass = SourceData.ShakeClass;
					if (ShakeClass.Get() == nullptr)
					{
						ShakeClass = ShakeSourceComponent->CameraShake;
					}

					if (ShakeClass.Get() != nullptr)
					{
						// Don't stop shakes immediately... let them gracefully blend out.
						const bool bImmediately = false;

						// TODO-ludovic: this isn't exactly correct...
						// We could be stopping other shakes of the same type started by other means.. but doing
						// the correct thing would require storing multiple weak shake instance pointers mapped 
						// to multiple player controllers, themselves mapped to multiple bound objects. Let's only
						// do that if we run into the (quite unlikely) case where we need it?
						ShakeSourceComponent->StopAllCameraShakesOfType(ShakeClass, bImmediately);

#if WITH_EDITOR
						UCameraModifier_CameraShake* const PreviewCameraShake = InstanceData.Previewer.GetCameraShake();
						PreviewCameraShake->RemoveAllCameraShakesOfClassFromSource(ShakeClass, ShakeSourceComponent, bImmediately);
#endif
					}
				}
			}

			// TODO-ludovic: only support a single binding for now.
			break;
		}
	}
};

FMovieSceneCameraShakeSourceShakeSectionTemplate::FMovieSceneCameraShakeSourceShakeSectionTemplate()
{
}

FMovieSceneCameraShakeSourceShakeSectionTemplate::FMovieSceneCameraShakeSourceShakeSectionTemplate(const UMovieSceneCameraShakeSourceShakeSection& Section)
	: SourceData(Section.ShakeData)
	, SectionStartTime(Section.HasStartFrame() ? Section.GetInclusiveStartFrame() : 0)
	, SectionEndTime(Section.HasEndFrame() ? Section.GetExclusiveEndFrame() : 0)
{
}

void FMovieSceneCameraShakeSourceShakeSectionTemplate::SetupOverrides()
{
	EnableOverrides(RequiresSetupFlag | RequiresTearDownFlag);
}

void FMovieSceneCameraShakeSourceShakeSectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FCameraShakeSourceShakeSectionInstanceData& InstanceData = PersistentData.AddSectionData<FCameraShakeSourceShakeSectionInstanceData>();
	InstanceData.Status = ECameraShakeSourceShakeStatus::NotStarted;

#if WITH_EDITOR
	InstanceData.Previewer.RegisterViewModifier();
#endif
}

void FMovieSceneCameraShakeSourceShakeSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FCameraShakeSourceShakeSectionInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceShakeSectionInstanceData>();

	const bool bHasDuration = InstanceData.Duration.IsSet();
	const bool bHasBlendOutTime = InstanceData.BlendOutTime.IsSet();

	if (InstanceData.Status == ECameraShakeSourceShakeStatus::NotStarted)
	{
		const FFrameTime ShakeEndTime = bHasDuration ? 
			SectionStartTime + InstanceData.Duration.GetValue() : SectionEndTime;

		if (SectionStartTime <= Context.GetTime() && Context.GetTime() <= ShakeEndTime)
		{
			// We haven't started playing the shake yet... add an execution token which will trigger it.
			ExecutionTokens.Add(FCameraShakeSourceShakeStartExecutionToken(SourceData));
			InstanceData.Status = ECameraShakeSourceShakeStatus::Started;
		}
	}
	else if (InstanceData.Status == ECameraShakeSourceShakeStatus::Started)
	{
		// We have started playing, but we might have a blend-out and/or end time that we need to 
		// watch out for. There can be 3 situations here:
		//
		//   1. The shake has a duration, and our source section is long enough that the shake will
		//		blend out and finish naturally on its own. In this case we have nothing to do 
		//		except update our internal status.
		//	 2. The shake has a duration, but our source section's size is cutting this short. We 
		//	    will want to start making the shake blend out manually, or stop it abruptely if
		//	    it doesn't have any blend-out time.
		//	 3. The shake has no duration, so we need to make it blend out manually near the end of
		//	    our source section, or end it abruptely at the end of our source section.
		//
		// Cases 2 and 3 require us to blend-out/stop the shake ourselves. Let's see which case we 
		// are in.
		//
		bool bObserveEndState = true;

		if (bHasBlendOutTime)
		{
			const bool bNeedsManualStop = 
				// Case 3
				!bHasDuration ||
				// Case 2
				(SectionStartTime + InstanceData.Duration.GetValue()) > SectionEndTime;

			if (bNeedsManualStop)
			{
				// Let's see if we have reached the time when we need to start blending out.
				const FFrameTime BlendOutStartTime = SectionEndTime - InstanceData.BlendOutTime.GetValue();
				if (Context.GetPreviousTime() <= BlendOutStartTime && Context.GetTime() >= BlendOutStartTime)
				{
					ExecutionTokens.Add(FCameraShakeSourceShakeBlendOutExecutionToken(SourceData));
					InstanceData.Status = ECameraShakeSourceShakeStatus::BlendingOut;
				}

				bObserveEndState = false;
			}
		}
		// else: If there's no blend-out, the shake always ends in an abrupt way.
		//	 In cases 2 and 3, this happens at the end of the section, so we handle that in the 
		//	 pre-animated restore token.
		//	 In case 1, the shake ends abruptly by itself, so we just need to observe when that
		//	 happens to keep our internal status correct.

		if (bObserveEndState && bHasDuration)
		{
			// Just observe when the shake has ended, in order to update our internal status.
			const FFrameTime ShakeEndTime = SectionStartTime + InstanceData.Duration.GetValue();
			if (Context.GetTime() >= ShakeEndTime)
			{
				InstanceData.Status = ECameraShakeSourceShakeStatus::NotStarted;
			}
		}
	}
	else if (InstanceData.Status == ECameraShakeSourceShakeStatus::BlendingOut)
	{
		// We're blending out... see if we have finished, so we can keep our internal status correct.
		if (bHasDuration)
		{
			const FFrameTime ShakeEndTime = SectionStartTime + InstanceData.Duration.GetValue();
			if (Context.GetTime() >= ShakeEndTime)
			{
				InstanceData.Status = ECameraShakeSourceShakeStatus::NotStarted;
			}
		}
	}

#if WITH_EDITOR
	const float DeltaTime = Context.GetFrameRate().AsSeconds(Context.GetDelta());
	const bool bIsPlaying = Context.GetStatus() == EMovieScenePlayerStatus::Playing;
	InstanceData.Previewer.Update(DeltaTime, bIsPlaying);
#endif
}

void FMovieSceneCameraShakeSourceShakeSectionTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FCameraShakeSourceShakeSectionInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceShakeSectionInstanceData>();

#if WITH_EDITOR
	InstanceData.Previewer.UnRegisterViewModifier();
#endif
}

