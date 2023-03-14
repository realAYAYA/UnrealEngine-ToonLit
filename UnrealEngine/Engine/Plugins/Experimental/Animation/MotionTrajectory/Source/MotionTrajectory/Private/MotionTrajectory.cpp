// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionTrajectory.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "MotionTrajectory"

DEFINE_LOG_CATEGORY(LogMotionTrajectory);

void FMotionTrajectoryModule::StartupModule()
{
}

void FMotionTrajectoryModule::ShutdownModule()
{
}

UMotionTrajectoryComponent::UMotionTrajectoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

FTrajectorySample UMotionTrajectoryComponent::CalcWorldSpacePresentTrajectorySample(float DeltaTime) const
{
	const APawn* Pawn = TryGetOwnerPawn();
	checkf(false, TEXT("UMotionTrajectoryComponent::GetPresentTrajectory for Pawn: %s requires implementation."), Pawn ? *(Pawn->GetHumanReadableName()) : TEXT("NULL"));
	return FTrajectorySample();
}

void UMotionTrajectoryComponent::TickTrajectory(float DeltaTime)
{
	// Compute the instantaneous/present trajectory sample and guarantee that is zeroed on all accumulated domains
	PresentTrajectorySampleWS = CalcWorldSpacePresentTrajectorySample(DeltaTime);
	PresentTrajectorySampleWS.AccumulatedDistance = 0.f;
	PresentTrajectorySampleWS.AccumulatedSeconds = 0.f;

	// convert to local space
	PresentTrajectorySampleLS = PresentTrajectorySampleWS;
	PresentTrajectorySampleLS.PrependOffset(PresentTrajectorySampleWS.Transform.Inverse(), 0.0f);


	// Tick the historical sample retention/decay algorithm by one iteration
	TickHistoryEvictionPolicy();
}

const APawn* UMotionTrajectoryComponent::TryGetOwnerPawn() const
{
	const AActor* Actor = GetOwner();
	return Actor ? Cast<APawn>(Actor) : nullptr;
}

FTrajectorySampleRange UMotionTrajectoryComponent::CombineHistoryPresentPrediction(bool bIncludeHistory, const FTrajectorySampleRange& Prediction) const
{
	FTrajectorySampleRange Trajectory;
	int32 TotalSampleCount = 1 + Prediction.Samples.Num(); // present + future

	FTrajectorySampleRange History;
	if (bIncludeHistory)
	{
		History = GetHistory();
		TotalSampleCount += History.Samples.Num();
	}

	// Linearly combines all uniformly sampled trajectory ranges from [0...n (samples)] in domain ordering:
	// Ex: [Past + Present + Future], with negative values representing the past, a zeroed present, and positive values representing the future
	Trajectory.Samples.Reserve(TotalSampleCount);
	Trajectory.Samples.Append(History.Samples);
	Trajectory.Samples.Add(PresentTrajectorySampleLS);
	Trajectory.Samples.Append(Prediction.Samples);
	Trajectory.SampleRate = SampleRate;
#if WITH_EDITORONLY_DATA
	Trajectory.DebugDrawTrajectory(bDebugDrawTrajectory, GetOwner()->GetWorld(), PreviousWorldTransform);
#endif
	return Trajectory;
}

void UMotionTrajectoryComponent::TickHistoryEvictionPolicy()
{
	// World space transform and time are used for determining local-relative historical samples
	const UWorld* World = GetWorld();
	const FTransform WorldTransform = PresentTrajectorySampleWS.Transform;
	const float WorldGameTime = UKismetSystemLibrary::GetGameTimeInSeconds(World);

	// Skip on the very first sample
	if (PreviousWorldGameTime != 0.f)
	{
		// Compute world space/time deltas for historical sample decay
		const float DeltaSeconds = WorldGameTime - PreviousWorldGameTime;
		const float DeltaDistance = FVector::Distance(WorldTransform.GetLocation(), PreviousWorldTransform.GetLocation());
		const FTransform DeltaTransform = PreviousWorldTransform.GetRelativeTransform(WorldTransform);

		for (auto& Sample : SampleHistory)
		{
			Sample.PrependOffset(DeltaTransform, -DeltaSeconds);
		}

		const FTrajectorySample FirstSample = SampleHistory.First();
		const float FirstSampleTime = FirstSample.AccumulatedSeconds;

		constexpr int32 DistanceDomainMask = static_cast<int32>(ETrajectorySampleDomain::Distance);
		constexpr int32 TimeDomainMask = static_cast<int32>(ETrajectorySampleDomain::Time);
		const bool bDistanceDomainEnabled = (HistorySettings.Domain & DistanceDomainMask) == DistanceDomainMask;
		const bool bTimeDomainEnabled = (HistorySettings.Domain & TimeDomainMask) == TimeDomainMask;

		// For both time and distance history domains, we compute the concept of an "effective" time horizon:
		// This value is present to guarantee uniform history decay against a fixed anchor point in the past when there is lack of trajectory motion
		// The anchor point is defined by the AccumulatedTime of the furthest history sample at the exact moment of zero motion
		if (FMath::IsNearlyZero(DeltaDistance) && !FirstSample.IsZeroSample())
		{
			if (EffectiveTimeDomain == 0.f)
			{
				// One time initialization of an effective time domain at the exact moment of zero motion
				EffectiveTimeDomain = FirstSampleTime;
			}
		}
		else
		{
			// When we are in motion, an effective time domain is not required
			EffectiveTimeDomain = 0.f;
		}

		// Remove all trajectory samples which are outside the boundaries of the enabled domain horizons
		SampleHistory.RemoveAll([&](const FTrajectorySample& Sample)
			{
				// Remove superfluous zero motion samples
				if (Sample.IsZeroSample() && PresentTrajectorySampleLS.IsZeroSample())
				{
					return true;
				}

				// Distance horizon
				if (bDistanceDomainEnabled && (Sample.AccumulatedDistance < -HistorySettings.Distance))
				{
					return true;
				}

				// Time horizon
				if (bTimeDomainEnabled && (Sample.AccumulatedSeconds < -HistorySettings.Seconds))
				{
					return true;
				}

				// Effective time horizon
				if (EffectiveTimeDomain != 0.f&& (Sample.AccumulatedSeconds < EffectiveTimeDomain))
				{
					return true;
				}

				return false;
			});
	}

	SampleHistory.Emplace(PresentTrajectorySampleLS);
	
	// Cache the present world space sample as the next frame's reference point for delta computation
	PreviousWorldTransform = WorldTransform;
	PreviousWorldGameTime = WorldGameTime;
}

void UMotionTrajectoryComponent::FlushHistory()
{
	SampleHistory.Reset();
	PreviousWorldTransform = {};
	PreviousWorldGameTime = 0.f;
	EffectiveTimeDomain = 0.f;
}

void UMotionTrajectoryComponent::OnComponentCreated()
{
	FlushHistory();
	SampleHistory.Reserve(MaxSamples);
	Super::OnComponentCreated();
}

void UMotionTrajectoryComponent::BeginPlay() 
{
	Super::BeginPlay();

	FTransform PresentTransform = FTransform::Identity;

	if (const APawn* Pawn = TryGetOwnerPawn())
	{
		PresentTransform = Pawn->GetActorTransform();
	}

	PresentTrajectorySampleWS = FTrajectorySample();
	PresentTrajectorySampleWS.Transform = PresentTransform;

	PresentTrajectorySampleLS = FTrajectorySample();
}

void UMotionTrajectoryComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickTrajectory(DeltaTime);
}

FTrajectorySampleRange UMotionTrajectoryComponent::GetTrajectory() const
{
	const APawn* Pawn = TryGetOwnerPawn();
	checkf(false, TEXT("UMotionTrajectoryComponent::GetTrajectory for Pawn: %s requires implementation."), Pawn ? *(Pawn->GetHumanReadableName()) : TEXT("NULL"));
	return FTrajectorySampleRange(SampleRate);
}

FTrajectorySampleRange UMotionTrajectoryComponent::GetTrajectoryWithSettings(const FMotionTrajectorySettings& Settings, bool bIncludeHistory) const
{
	const APawn* Pawn = TryGetOwnerPawn();
	checkf(false, TEXT("UMotionTrajectoryComponent::GetTrajectoryWithSettings for Pawn: %s requires implementation."), Pawn ? *(Pawn->GetHumanReadableName()) : TEXT("NULL"));
	return FTrajectorySampleRange(SampleRate);
}

void UMotionTrajectoryComponent::SetSampleRate(int32 Rate)
{
	// To maintain trajectory sample coherency, changing the sample rate will cause all historical samples to be flushed
	// In the future, we may consider leveraging the frame-rate independent uniform resampling algorithm implemented within GetHistory()
	FlushHistory();
	SampleRate = Rate;
}

FTrajectorySampleRange UMotionTrajectoryComponent::GetHistory() const
{
	if (SampleHistory.IsEmpty())
	{
		return FTrajectorySampleRange(SampleRate);
	}
	else if (bUniformSampledHistory)
	{
		// Establish a uniform history range that will be frame-rate independent, and resampled against the SampleRate property specified by this component
		FTrajectorySampleRange UniformHistory;
		UniformHistory.Samples.Reserve(SampleHistory.Num());
		UniformHistory.SampleRate = SampleRate;

		int32 InterpStartIdx = 0;
		const float SampleFrequency = 1.f / static_cast<float>(SampleRate);
		const float FirstNonUniformSampleTime = SampleHistory.First().AccumulatedSeconds;
		const float FirstUniformSampleTime = FMath::CeilToFloat(FirstNonUniformSampleTime / SampleFrequency) * SampleFrequency;

		check(SampleFrequency > 0.f);
		check(FirstUniformSampleTime <= 0.f);

		// Iterate the source history buffer with a uniform sample frequency
		// The iteration exit condition is meant to guarantee that we do not include (erroneously) a instantaneous/present trajectory sample at zero time
		for (int32 Idx = 0;; ++Idx)
		{
			const float SampleTime = FirstUniformSampleTime + static_cast<float>(Idx) * SampleFrequency;
			if (SampleTime < -UE_KINDA_SMALL_NUMBER)
			{
				const FTrajectorySample Sample = FTrajectorySampleRange::IterSampleTrajectory(SampleHistory, ETrajectorySampleDomain::Time, SampleTime, InterpStartIdx, bSmoothInterpolation);
				UniformHistory.Samples.Emplace(Sample);
			}
			else
			{
				break;
			}
		}

		return UniformHistory;
	}
	else
	{
		const int32 HistorySampleCount = SampleHistory.Num();
		const int32 PresentSampleIdx = HistorySampleCount - 1;

		// Convert the source history ring buffer into a trajectory range
		// We consider these values to be non-uniform with a variable, frame-rate dependent sample rate governed by the component tick rate
		FTrajectorySampleRange SourceHistory;
		SourceHistory.Samples.Reserve(HistorySampleCount);

		for (const auto& Sample : SampleHistory)
		{
			SourceHistory.Samples.Emplace(Sample);
		}

		// Remove the present trajectory sample, this will not be included in the history
		check(FMath::IsNearlyZero(SourceHistory.Samples[PresentSampleIdx].AccumulatedSeconds));
		SourceHistory.Samples.RemoveAt(PresentSampleIdx);

		return SourceHistory;
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMotionTrajectoryModule, MotionTrajectory)