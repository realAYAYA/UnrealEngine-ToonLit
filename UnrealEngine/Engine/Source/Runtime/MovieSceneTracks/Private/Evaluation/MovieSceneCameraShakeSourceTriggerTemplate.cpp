// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneCameraShakeSourceTriggerTemplate.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeSourceComponent.h"
#include "Channels/MovieSceneCameraShakeSourceTriggerChannel.h"
#include "Evaluation/MovieSceneCameraShakePreviewer.h"
#include "Sections/MovieSceneCameraShakeSourceTriggerSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneCameraShakeSourceTriggerTemplate)

struct FCameraShakeSourceTriggerInstanceData : IPersistentEvaluationData
{
#if WITH_EDITOR
	FCameraShakePreviewer Previewer;
#endif
};

struct FCameraShakeSourceTriggerExecutionToken : IMovieSceneExecutionToken
{
	TArray<FMovieSceneCameraShakeSourceTrigger> Triggers;

	FCameraShakeSourceTriggerExecutionToken(TArray<FMovieSceneCameraShakeSourceTrigger>&& InTriggers)
		: Triggers(MoveTemp(InTriggers))
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FCameraShakeSourceTriggerInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceTriggerInstanceData>();

		for (TWeakObjectPtr<> BoundObject : Player.FindBoundObjects(Operand))
		{
			if (BoundObject.IsValid())
			{
				if (UCameraShakeSourceComponent* ShakeSourceComponent = CastChecked<UCameraShakeSourceComponent>(BoundObject.Get()))
				{
					for (const FMovieSceneCameraShakeSourceTrigger& Trigger : Triggers)
					{
						TSubclassOf<UCameraShakeBase> ShakeClass = Trigger.ShakeClass;
						if (ShakeClass.Get() == nullptr)
						{
							ShakeClass = ShakeSourceComponent->CameraShake;
						}

						if (ShakeClass.Get() != nullptr)
						{
							// Start playing the shake.
							ShakeSourceComponent->StartCameraShake(ShakeClass, Trigger.PlayScale, Trigger.PlaySpace, Trigger.UserDefinedPlaySpace);

#if WITH_EDITOR
							// Also start playing the shake in our editor preview.
							UCameraModifier_CameraShake* const PreviewCameraShake = InstanceData.Previewer.GetCameraShake();

							FAddCameraShakeParams Params;
							Params.SourceComponent = ShakeSourceComponent;
							Params.Scale = Trigger.PlayScale;
							Params.PlaySpace = Trigger.PlaySpace;
							Params.UserPlaySpaceRot = Trigger.UserDefinedPlaySpace;
							PreviewCameraShake->AddCameraShake(ShakeClass, Params);
#endif
						}
					}
				}
			}

			// TODO-ludovic: only support a single binding for now.
			break;
		}
	}
};


FMovieSceneCameraShakeSourceTriggerSectionTemplate::FMovieSceneCameraShakeSourceTriggerSectionTemplate(const UMovieSceneCameraShakeSourceTriggerSection& Section)
{
	TMovieSceneChannelData<const FMovieSceneCameraShakeSourceTrigger> TriggerData = Section.GetChannel().GetData();
	TArrayView<const FFrameNumber> Times = TriggerData.GetTimes();
	TArrayView<const FMovieSceneCameraShakeSourceTrigger> Values = TriggerData.GetValues();

	TriggerTimes.Reserve(Times.Num());
	TriggerValues.Reserve(Values.Num());

	for (int32 Idx = 0; Idx < Times.Num(); ++Idx)
	{
		TriggerTimes.Add(Times[Idx]);
		TriggerValues.Add(Values[Idx]);
	}
}

void FMovieSceneCameraShakeSourceTriggerSectionTemplate::SetupOverrides()
{
	EnableOverrides(RequiresSetupFlag | RequiresTearDownFlag);
}

void FMovieSceneCameraShakeSourceTriggerSectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FCameraShakeSourceTriggerInstanceData& InstanceData = PersistentData.AddSectionData<FCameraShakeSourceTriggerInstanceData>();
#if WITH_EDITOR
	InstanceData.Previewer.RegisterViewModifier();
#endif
}

void FMovieSceneCameraShakeSourceTriggerSectionTemplate::EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
#if WITH_EDITOR
	{
		const float DeltaTime = Context.GetFrameRate().AsSeconds(Context.GetDelta());
		const bool bIsPlaying = Context.GetStatus() == EMovieScenePlayerStatus::Playing;
		FCameraShakeSourceTriggerInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceTriggerInstanceData>();
		InstanceData.Previewer.Update(DeltaTime, bIsPlaying);
	}
#endif

	if (Context.GetStatus() == EMovieScenePlayerStatus::Stopped || Context.IsSilent())
	{
		return;
	}

	const bool bBackwards = Context.GetDirection() == EPlayDirection::Backwards;
	if (bBackwards)
	{
		return;
	}

	TArray<FMovieSceneCameraShakeSourceTrigger> ShakesToTrigger;

	const int32 TriggerCount = TriggerTimes.Num();
	const float PositionInSeconds = Context.GetTime() * Context.GetRootToSequenceTransform().InverseLinearOnly() / Context.GetFrameRate();

	for (int32 KeyIndex = 0; KeyIndex < TriggerCount; ++KeyIndex)
	{
		const FFrameNumber Time = TriggerTimes[KeyIndex];
		if (SweptRange.Contains(Time))
		{
			ShakesToTrigger.Add(TriggerValues[KeyIndex]);
		}
	}

	if (ShakesToTrigger.Num() > 0)
	{
		ExecutionTokens.Add(FCameraShakeSourceTriggerExecutionToken(MoveTemp(ShakesToTrigger)));
	}
}

FMovieSceneAnimTypeID FMovieSceneCameraShakeSourceTriggerSectionTemplate::GetAnimTypeID()
{
	return TMovieSceneAnimTypeID<FMovieSceneCameraShakeSourceTriggerSectionTemplate>();
}

void FMovieSceneCameraShakeSourceTriggerSectionTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FCameraShakeSourceTriggerInstanceData& InstanceData = PersistentData.GetSectionData<FCameraShakeSourceTriggerInstanceData>();
#if WITH_EDITOR
	InstanceData.Previewer.UnRegisterViewModifier();
#endif
}


