// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionTrajectoryLibrary.h"
#include "MotionTrajectory.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Algo/Find.h"
#include "GameFramework/Actor.h"

static void FlattenTrajectoryPosition(
	FTrajectorySample& Sample, 
	const FTrajectorySample& PrevSample, 
	const FTrajectorySample& FlattenedPrevSample, 
	bool PreserveSpeed)
{
	if (!Sample.Transform.GetTranslation().IsZero())
	{
		const FVector Translation = Sample.Transform.GetLocation() - PrevSample.Transform.GetLocation();
		const FVector FlattenedTranslation = FVector(Translation.X, Translation.Y, 0.0f);

		if (PreserveSpeed)
		{
			const float TargetDistance = 
				FMath::Abs(Sample.AccumulatedDistance - PrevSample.AccumulatedDistance);
			const FVector FlattenedTranslationDir = Translation.GetSafeNormal2D();

			// Take the full displacement, effectively meaning that the Z axis never existed
			Sample.Transform.SetLocation(
				FlattenedPrevSample.Transform.GetLocation() + (FlattenedTranslationDir * TargetDistance));
		}
		else
		{
			// Accumulate the delta displacement difference as a result of Z axis being removed
			const float DeltaSeconds = Sample.AccumulatedSeconds - PrevSample.AccumulatedSeconds;
			const float DeltaDistance = DeltaSeconds >= 0.0f ?
				FlattenedTranslation.Size() :
				-FlattenedTranslation.Size();

			Sample.AccumulatedDistance = FlattenedPrevSample.AccumulatedDistance + DeltaDistance;
			Sample.Transform.SetTranslation(FlattenedPrevSample.Transform.GetLocation() + FlattenedTranslation);
		}
	}
}

FTrajectorySampleRange UMotionTrajectoryBlueprintLibrary::FlattenTrajectory2D(FTrajectorySampleRange Trajectory, bool bPreserveSpeed)
{

	if (!Trajectory.HasSamples())
	{
		return Trajectory;
	}

	if (Trajectory.HasOnlyZeroSamples())
	{
		return Trajectory;
	}

	// Each iteration will preserve the linear magnitudes of velocity and acceleration while removing the direction Z-axis component
	for (auto& Sample : Trajectory.Samples)
	{
		// Note: As as a consequence of magnitude preservation, AccumulatedDistance alongside AccumulatedTime should not need modification

		// Linear velocity Z-axis component removal
		if (!Sample.LinearVelocity.IsZero())
		{
			const float VelMagnitude = bPreserveSpeed ? Sample.LinearVelocity.Size() : Sample.LinearVelocity.Size2D();
			Sample.LinearVelocity = VelMagnitude * Sample.LinearVelocity.GetSafeNormal2D();
		}
	}

	// The present position sample is used as the basis for recomputing the future and history Accumulated Distance
	const int32 PresentSampleIdx = Trajectory.Samples.IndexOfByPredicate([](const FTrajectorySample& Sample){
		return Sample.AccumulatedSeconds == 0.f;
	});

	check(PresentSampleIdx != INDEX_NONE);

	// the present location should be zero but let's, for sanity, assume it might not be
	FTrajectorySample& PresentSample = Trajectory.Samples[PresentSampleIdx];
	const FVector PresentSampleLocation = PresentSample.Transform.GetLocation();
	PresentSample.Transform.SetLocation(FVector(PresentSampleLocation.X, PresentSampleLocation.Y, 0.0f));

	// Walk all samples into the future, conditionally removing contribution of Z axis motion
	FTrajectorySample PrevSample = PresentSample;
	for (int32 Idx = PresentSampleIdx + 1, Num = Trajectory.Samples.Num(); Idx < Num; ++Idx)
	{
		FTrajectorySample CurrentSample = Trajectory.Samples[Idx];
		FlattenTrajectoryPosition(Trajectory.Samples[Idx], PrevSample, Trajectory.Samples[Idx - 1], bPreserveSpeed);
		PrevSample = CurrentSample;
	}

	// There is a possibility history has not been computed yet
	if (PresentSampleIdx == 0)
	{
		return Trajectory;
	}

	// Walk all samples in the past, conditionally removing the contribution of Z axis motion
	PrevSample = PresentSample;
	for (int32 Idx = PresentSampleIdx - 1, Begin = 0; Idx >= Begin; --Idx)
	{
		FTrajectorySample CurrentSample = Trajectory.Samples[Idx];
		FlattenTrajectoryPosition(Trajectory.Samples[Idx], PrevSample, Trajectory.Samples[Idx + 1], bPreserveSpeed);
		PrevSample = CurrentSample;
	}

	return Trajectory;
}

static FVector ClampDirection(const FVector InputVector, const TArray<FTrajectoryDirectionClamp>& Directions)
{
	if (Directions.IsEmpty())
	{
		return InputVector;
	}

	FVector InputDirection;
	float InputLength;
	InputVector.ToDirectionAndLength(InputDirection, InputLength);
	if (InputLength < SMALL_NUMBER)
	{
		return InputVector;
	}

	// Assume first direction is best then check if the input direction is within the remaining sectors
	FVector NearestDirection = Directions[0].Direction;
	for (int32 DirIdx = 1; DirIdx != Directions.Num(); ++DirIdx)
	{
		const auto& Clamp = Directions[DirIdx];
		if (FMath::Acos(FVector::DotProduct(InputDirection, Clamp.Direction)) < FMath::DegreesToRadians(Clamp.AngleTresholdDegrees))
		{
			NearestDirection = Clamp.Direction;
			break;
		}
	}

	const FVector Output = InputLength * NearestDirection;
	return Output;
}

FTrajectorySampleRange UMotionTrajectoryBlueprintLibrary::ClampTrajectoryDirection(FTrajectorySampleRange Trajectory, const TArray<FTrajectoryDirectionClamp>& Directions, bool bPreserveRotation)
{
	if (Directions.IsEmpty())
	{
		return Trajectory;
	}

	if (!Trajectory.HasSamples())
	{
		return Trajectory;
	}

	if (Trajectory.HasOnlyZeroSamples())
	{
		return Trajectory;
	}

	// The clamped present (zero domain) sample is used as the basis for projecting samples along its trajectory
	FTrajectorySample* PresentSample = Algo::FindByPredicate(Trajectory.Samples, [](const FTrajectorySample& Sample) {
		return FMath::IsNearlyZero(Sample.AccumulatedSeconds);
	}
	);

	check(PresentSample)

	if (!PresentSample->LinearVelocity.IsZero())
	{
		const FVector VelocityBasis = ClampDirection(PresentSample->LinearVelocity, Directions).GetSafeNormal();

		for (auto& Sample : Trajectory.Samples)
		{
			// Align linear velocity onto the velocity basis to maintain the present intended direction, while retaining per-sample magnitude
			if (!Sample.LinearVelocity.IsZero())
			{
				Sample.LinearVelocity = Sample.LinearVelocity.Size() * Sample.LinearVelocity.ProjectOnTo(VelocityBasis).GetSafeNormal();
			}

			// Align the position path through projection onto the modified velocity
			if (!Sample.LinearVelocity.IsZero() && !Sample.Transform.GetLocation().IsZero())
			{
				Sample.Transform.SetLocation(
					FMath::Abs(Sample.AccumulatedDistance) * 
					Sample.Transform.GetLocation().ProjectOnTo(Sample.LinearVelocity).GetSafeNormal());
			}

			if (bPreserveRotation)
			{
				Sample.Transform.SetRotation(PresentSample->Transform.GetRotation());
			}
		}
	}

	return Trajectory;
}

FTrajectorySampleRange UMotionTrajectoryBlueprintLibrary::RotateTrajectory(
	FTrajectorySampleRange Trajectory, 
	const FQuat& Rotation)
{
	Trajectory.Rotate(Rotation);
	return Trajectory;
}

FTrajectorySampleRange UMotionTrajectoryBlueprintLibrary::MakeTrajectoryRelativeToComponent(
	FTrajectorySampleRange ActorTrajectory, 
	const USceneComponent* Component)
{
	if (!IsValid(Component))
	{
		UE_LOG(LogMotionTrajectory, Error, TEXT("Invalid component!"));
		return ActorTrajectory;
	}

	const AActor* Owner = Component->GetOwner();
	const FTransform OwnerTransformWS = Owner->GetActorTransform();
	const FTransform ComponentTransformWS = Component->GetComponentTransform();
	const FTransform ReferenceChangeTransform = OwnerTransformWS.GetRelativeTransform(ComponentTransformWS);

	ActorTrajectory.TransformReferenceFrame(ReferenceChangeTransform);
	return ActorTrajectory;
}

void UMotionTrajectoryBlueprintLibrary::DebugDrawTrajectory(const AActor* Actor
	, const FTransform& WorldTransform
	, const FTrajectorySampleRange& Trajectory
	, const FLinearColor PredictionColor
	, const FLinearColor HistoryColor
	, float TransformScale
	, float TransformThickness
	, float ArrowScale
	, float ArrowSize
	, float ArrowThickness
)
{
	if (Actor)
	{
		Trajectory.DebugDrawTrajectory(true
			, Actor->GetWorld()
			, WorldTransform.IsValid() ? WorldTransform : FTransform::Identity
			, PredictionColor
			, HistoryColor
			, TransformScale
			, TransformThickness
			, ArrowScale
			, ArrowSize
			, ArrowThickness
		);
	}
}

bool UMotionTrajectoryBlueprintLibrary::IsStoppingTrajectory(
	const FTrajectorySampleRange& Trajectory, 
	float MoveMinSpeed,
	float IdleMaxSpeed)
{
	if (Trajectory.Samples.Num() > 0)
	{
		const FVector LastLinearVelocity = Trajectory.Samples.Last().LinearVelocity;
		const float SquaredLastLinearSpeed = LastLinearVelocity.SquaredLength();
		
		int StartIdx = 0;
		const FTrajectorySample& PresentSample = FTrajectorySampleRange::IterSampleTrajectory(
			Trajectory.Samples, 
			ETrajectorySampleDomain::Time, 
			0.0f, 
			StartIdx);
		const FVector PresenLinearVelocity = PresentSample.LinearVelocity;
		const float SquaredPresentLinearSpeed = PresenLinearVelocity.SquaredLength();

		const bool bIsStopping =
			(SquaredPresentLinearSpeed >= (MoveMinSpeed * MoveMinSpeed)) &&
			(SquaredLastLinearSpeed <= (IdleMaxSpeed * IdleMaxSpeed));

		return bIsStopping;
	}

	return false;
}

bool UMotionTrajectoryBlueprintLibrary::IsStartingTrajectory(
	const FTrajectorySampleRange& Trajectory,
	float MoveMinSpeed,
	float IdleMaxSpeed)
{
	if (Trajectory.Samples.Num() > 0)
	{
		const FVector FirstLinearVelocity = Trajectory.Samples[0].LinearVelocity;
		const float SquaredFirstLinearSpeed = FirstLinearVelocity.SquaredLength();

		int StartIdx = 0;
		const FTrajectorySample& PresentSample = FTrajectorySampleRange::IterSampleTrajectory(
			Trajectory.Samples,
			ETrajectorySampleDomain::Time,
			0.0f,
			StartIdx);
		const FVector PresentLinearVelocity = PresentSample.LinearVelocity;
		const float SquaredPresentLinearSpeed = PresentLinearVelocity.SquaredLength();

		const bool IsStarting =
			(SquaredPresentLinearSpeed >= (MoveMinSpeed * MoveMinSpeed)) &&
			(SquaredFirstLinearSpeed <= (IdleMaxSpeed * IdleMaxSpeed));

		return IsStarting;
	}

	return false;
}


bool UMotionTrajectoryBlueprintLibrary::IsConstantSpeedTrajectory(
	const FTrajectorySampleRange& Trajectory,
	float Speed,
	float Tolerance)
{
	if (Trajectory.Samples.Num() > 0)
	{
		const float MinSpeed = FMath::Max(Speed - Tolerance, 0.0f);
		const float SquaredMinSpeed = MinSpeed * MinSpeed;
		const float MaxSpeed = FMath::Max(Speed + Tolerance, 0.0f);
		const float SquaredMaxSpeed = MaxSpeed * MaxSpeed;

		auto IsWithinLimit = [=](float SquaredSpeed)
		{
			return SquaredSpeed >= SquaredMinSpeed && SquaredSpeed <= SquaredMaxSpeed;
		};

		const FVector LastLinearVelocity = Trajectory.Samples.Last().LinearVelocity;
		const float SquaredLastLinearSpeed = LastLinearVelocity.SquaredLength();
		const bool LastSpeedWithinLimit = IsWithinLimit(SquaredLastLinearSpeed);

		int StartIdx = 0;
		const FTrajectorySample& PresentSample = FTrajectorySampleRange::IterSampleTrajectory(
			Trajectory.Samples,
			ETrajectorySampleDomain::Time,
			0.0f,
			StartIdx);
		const FVector PresentLinearVelocity = PresentSample.LinearVelocity;
		const float SquaredPresentLinearSpeed = PresentLinearVelocity.SquaredLength();
		const bool PresentSpeedWithinLimit = IsWithinLimit(SquaredLastLinearSpeed);

		const bool bIsAtSpeed = LastSpeedWithinLimit && PresentSpeedWithinLimit;

		return bIsAtSpeed;
	}

	return false;
}

namespace UE::MotionTrajectory
{
	struct FTurnEvaluator
	{

	public:
		FTurnEvaluator(
			const FTrajectorySampleRange& InTrajectory, 
			ETrajectorySampleDomain InRotationConstraintDomain,
			float InRotationConstraintLimit,
			const FVector& InTurnAxis)
			: Trajectory(InTrajectory)
			, RotationConstraintDomain(InRotationConstraintDomain)
			, RotationConstraintLimit(InRotationConstraintLimit)
			, TurnAxis(InTurnAxis)
		{
			CalcRotSpeedToExtrapolate();
			CalcTurnData();
		}

		const FTrajectorySampleRange& Trajectory;
		ETrajectorySampleDomain RotationConstraintDomain;
		float RotationConstraintLimit;
		FVector TurnAxis;

		float RotSpeedToExtrapolate;
		TArray<float, TInlineAllocator<128>> AccumulatedTurnAxisRotations;
		TArray<float, TInlineAllocator<128>> ImmediateTurnAxisRotations;

		// Evaluates if there's a turn in the trajectory starting from SampleIdx. If the velocity and the alignment
		// between the forward axis and the velocity match when comparing the samples at SampleIdx and the end of the
		// trajectory, up to MaxAlignmentAngleDegrees, the function will return false. Otherwise, it returns false.
		bool IsTurning(int32 SampleIdx, const FVector& ForwardAxis, float MaxAlignmentAngleDegrees)
		{
			const FTrajectorySample CurrentSample = Trajectory.Samples[SampleIdx];

			const int32 LastSampleIdx = Trajectory.Samples.Num() - 1;
			const FTrajectorySample LastSample = Trajectory.Samples[LastSampleIdx];

			const float CurrentToLastRotSpeed =
				CalcRotationSpeed(SampleIdx, LastSampleIdx);
			const float CurrentSpeedDelta = CurrentToLastRotSpeed - RotSpeedToExtrapolate;
			const float CurrentAngleDelta = CurrentSpeedDelta * LastSample.AccumulatedSeconds;

			const FVector CurrentFwd = CurrentSample.Transform.GetRotation().RotateVector(ForwardAxis);
			const FQuat CurrentVelToFwdRotation = FQuat::FindBetweenVectors(CurrentSample.LinearVelocity, CurrentFwd);

			const FVector LastFwd = LastSample.Transform.GetRotation().RotateVector(ForwardAxis);
			const FQuat LastVelToFwdRotation = FQuat::FindBetweenVectors(LastSample.LinearVelocity, LastFwd);

			const FQuat VelAlignmentDelta = CurrentVelToFwdRotation.Inverse() * LastVelToFwdRotation;
			const float VelAlignmentDeltaAngle = VelAlignmentDelta.GetTwistAngle(TurnAxis);

			const float MaxAlignmentAngleRadians = FMath::DegreesToRadians(MaxAlignmentAngleDegrees);
			bool bIsAligned =
				(FMath::Abs(CurrentAngleDelta) < MaxAlignmentAngleRadians) &&
				(FMath::Abs(VelAlignmentDeltaAngle) < MaxAlignmentAngleRadians);

			return !bIsAligned;
		}

		bool FindTurnBeyondExtrapolation(float MinSharpTurnAngleDegrees)
		{
			check(Trajectory.Samples.Num() > 1);

			const float MinSharpTurnAngleRadians = FMath::DegreesToRadians(MinSharpTurnAngleDegrees);

			const int32 LastSampleIdx = Trajectory.Samples.Num() - 1;
			const float FirstToLastRotSpeed = CalcRotationSpeed(0, LastSampleIdx);

			const float SignLastDirRotSpeed = RotSpeedToExtrapolate >= 0 ? 1.0f : -1.0f; // FMath::Sign(RotSpeedToExtrapolate);
			const float SignedFirstToLastRotSpeed = SignLastDirRotSpeed * FirstToLastRotSpeed;

			// first we'll look for the most aggressive turning frame with respect to the circling speed
			float FastestRelativeTurnSpeed = 0.0f;
			int32 FastestRelativeTurnSampleIdx = FindFastestRelativeSpeedSample(FastestRelativeTurnSpeed);
			if (FastestRelativeTurnSampleIdx == INDEX_NONE)
			{
				return false;
			}

			// Starting from the root sample containing the fastest rotation speed, grow the evaluated interval, from
			// StartSampleIdx to EndSampleIdx, in a greedy approach. This should lead to the shortest interval that 
			// contains a large enough turn
			const FTrajectorySample& RootSample = Trajectory.Samples[FastestRelativeTurnSampleIdx];
			int32 EndSampleIdx = FastestRelativeTurnSampleIdx;
			int32 StartSampleIdx = EndSampleIdx - 1;
			const float TurnSpeedIntervalRadius = RotSpeedToExtrapolate / 2.0f;
			bool bDone = false;
			while (!bDone)
			{
				const float CurrentTurnLength = 
					GetAccumulatedDomainValue(Trajectory.Samples[EndSampleIdx]) - 
					GetAccumulatedDomainValue(Trajectory.Samples[StartSampleIdx]);

				const int32 PrevIdx = StartSampleIdx - 1;
				float PrevRelativeTurn = 0.0f;
				bool bPrevValid = IsSampleContributingToSharpTurn(
					StartSampleIdx,
					CurrentTurnLength,
					FastestRelativeTurnSpeed,
					PrevRelativeTurn);

				const int32 NextIdx = EndSampleIdx + 1;
				float NextRelativeTurn = 0.0f;
				bool bNextValid = IsSampleContributingToSharpTurn(
					NextIdx,
					CurrentTurnLength,
					FastestRelativeTurnSpeed,
					NextRelativeTurn);

				// Every iteration one of 3 things happen:
				// 1. The interval grows 1 sample into the past
				// 2. The interval grows 1 sample into the future
				// 3. The evaluation ends because either a large enough turn was found or there are no more valid
				//    samples to add to the interval
				if (bNextValid && bPrevValid)
				{
					// if both end points are contributing to the sharp turn, expand the interval towards the biggest
					// contribution
					if (FMath::Abs(NextRelativeTurn) >= FMath::Abs(PrevRelativeTurn))
					{
						EndSampleIdx = NextIdx;
					}
					else
					{
						StartSampleIdx = PrevIdx;
					}
				}
				else if (bNextValid)
				{
					EndSampleIdx = NextIdx;
				}
				else if (bPrevValid)
				{
					StartSampleIdx = PrevIdx;
				}
				else
				{
					// if none of the evaluated extremities contribute to the turn, stop increasing the turn interval.
					bDone = true;
				}

				const float ExtrapolatedTurn =
					RotSpeedToExtrapolate *
					(Trajectory.Samples[EndSampleIdx].AccumulatedSeconds -
					 Trajectory.Samples[StartSampleIdx].AccumulatedSeconds);
				const float TotalTurn =
					AccumulatedTurnAxisRotations[EndSampleIdx] - AccumulatedTurnAxisRotations[StartSampleIdx];

				if ((SignLastDirRotSpeed * TotalTurn) > (SignLastDirRotSpeed * ExtrapolatedTurn + MinSharpTurnAngleRadians) ||
					(SignLastDirRotSpeed * TotalTurn) < -MinSharpTurnAngleRadians)
				{
					// at any moment if a big enough turn is found, return true, there's no need to do anything else.
					return true;
				}
			}

			return false;
		}

	private:

		float CalcRotationSpeed(int32 StartIdx, int32 EndIdx)
		{
			const FTrajectorySample& SampleA = Trajectory.Samples[StartIdx];
			const FTrajectorySample& SampleB = Trajectory.Samples[EndIdx];
			const float TotalTime = SampleB.AccumulatedSeconds - SampleA.AccumulatedSeconds;
			const float TotalAngleDelta = AccumulatedTurnAxisRotations[EndIdx] - AccumulatedTurnAxisRotations[StartIdx];
			const float RotationSpeed = TotalAngleDelta / TotalTime;
			return RotationSpeed;
		}

		// Populate arrays with turn information. If these end up being used multiple times per frame, we should consider
		// adding this information to FTrajectorySample (maybe deferred until needed)
		void CalcTurnData()
		{
			AccumulatedTurnAxisRotations.SetNum(Trajectory.Samples.Num());
			AccumulatedTurnAxisRotations[0] = 0.0f;

			ImmediateTurnAxisRotations.SetNum(Trajectory.Samples.Num());
			ImmediateTurnAxisRotations[0] = 0.0f;

			const int32 NumSamples = Trajectory.Samples.Num();
			for (int32 SampleIdx = 1; SampleIdx < NumSamples; ++SampleIdx)
			{
				const FTrajectorySample& PrevSample = Trajectory.Samples[SampleIdx - 1];
				const FTrajectorySample& CurrSample = Trajectory.Samples[SampleIdx];

				FQuat VelocityDelta = FQuat::FindBetweenVectors(PrevSample.LinearVelocity, CurrSample.LinearVelocity);
				FVector DeltaAxis;
				float DeltaAngle = VelocityDelta.GetTwistAngle(TurnAxis);

				ImmediateTurnAxisRotations[SampleIdx] = DeltaAngle;
				AccumulatedTurnAxisRotations[SampleIdx] = AccumulatedTurnAxisRotations[SampleIdx - 1] + DeltaAngle;
			}
		}

		void CalcRotSpeedToExtrapolate()
		{
			check(Trajectory.Samples.Num() > 2);

			const int32 LastSampleIdx = Trajectory.Samples.Num() - 1;
			const FTrajectorySample& LastSample = Trajectory.Samples[LastSampleIdx];

			const int32 BeforeLastSampleIdx = LastSampleIdx - 1;
			const FTrajectorySample& BeforeLastSample = Trajectory.Samples[BeforeLastSampleIdx];

			const float LastDeltaSeconds = LastSample.AccumulatedSeconds - BeforeLastSample.AccumulatedSeconds;

			const FQuat LastDirectionRotation =
				FQuat::FindBetweenVectors(BeforeLastSample.LinearVelocity, LastSample.LinearVelocity);
			const float LastDirRotAngle = LastDirectionRotation.GetTwistAngle(TurnAxis);

			RotSpeedToExtrapolate = LastDirRotAngle / LastDeltaSeconds;
		}

		float GetAccumulatedDomainValue(const FTrajectorySample& Sample)
		{
			switch (RotationConstraintDomain)
			{
			case ETrajectorySampleDomain::Distance:
				return Sample.AccumulatedDistance;
			case ETrajectorySampleDomain::Time:
				return Sample.AccumulatedSeconds;
			}

			return 0.0f;
		}

		// A sample is contributing to the sharp turn if it is turning to the same side as FastestRelativeTurnSpeed,
		// if adding the sample does not cause the turn to grow larger than the specified constraint, and the turn is
		// either a. sharper than the extrapolated rotation speed or b. to the opposite side of the extrapolated
		// rotation speed.
		bool IsSampleContributingToSharpTurn(
			int32 SampleIdx,
			float CurrentTurnLength,
			float FastestRelativeTurnSpeed, 
			float& OutSampleRelativeTurn)
		{
			OutSampleRelativeTurn = 0.0f;

			bool bIsContributing = false;

			if (SampleIdx >= 1 && SampleIdx < Trajectory.Samples.Num())
			{
				const int32 PrevIdx = SampleIdx - 1;

				const FTrajectorySample& Sample = Trajectory.Samples[SampleIdx];
				const float StartDomainValue = GetAccumulatedDomainValue(Sample);

				const FTrajectorySample& PrevSample = Trajectory.Samples[PrevIdx];
				const float PrevDomainValue = GetAccumulatedDomainValue(PrevSample);

				const float SampleLengthContribution = StartDomainValue - PrevDomainValue;
				const float NewTurnLength = SampleLengthContribution + CurrentTurnLength;

				if ((RotationConstraintDomain == ETrajectorySampleDomain::None) ||
					(NewTurnLength <= RotationConstraintLimit))
				{
					const float DeltaTime =
						Trajectory.Samples[SampleIdx].AccumulatedSeconds -
						Trajectory.Samples[PrevIdx].AccumulatedSeconds;
					const float PrevTurnSpeed = ImmediateTurnAxisRotations[SampleIdx] / DeltaTime;

					const float TurnSpeedIntervalRadius = RotSpeedToExtrapolate / 2.0f;
					OutSampleRelativeTurn = PrevTurnSpeed - TurnSpeedIntervalRadius;
					bIsContributing =
						FMath::Sign(OutSampleRelativeTurn) == FMath::Sign(FastestRelativeTurnSpeed) &&
						FMath::Abs(OutSampleRelativeTurn) > FMath::Abs(TurnSpeedIntervalRadius);
				}
			}

			return bIsContributing;
		}

		int32 FindFastestRelativeSpeedSample(float& OutFastestRelativeTurnSpeed)
		{
			int32 FastestRelativeTurnSampleIdx = INDEX_NONE;
			OutFastestRelativeTurnSpeed = 0.0f;
			float AbsFastestRelativeTurnSpeed = 0.0f;

			// we'll compare turn speeds with the center of the speed interval [0; RotSpeedToInterpolate].
			// A turn that falls inside the interval will be disregarded.
			const float TurnSpeedIntervalRadius = RotSpeedToExtrapolate / 2.0f;
			const int32 NumSamples = Trajectory.Samples.Num();
			for (int32 SampleIdx = 1; SampleIdx < NumSamples; ++SampleIdx)
			{
				const float DeltaTime =
					Trajectory.Samples[SampleIdx].AccumulatedSeconds -
					Trajectory.Samples[SampleIdx - 1].AccumulatedSeconds;
				const float ImmediateSpeed = ImmediateTurnAxisRotations[SampleIdx] / DeltaTime;
				const float RelativeSpeed = ImmediateSpeed - TurnSpeedIntervalRadius;
				const float AbsRelativeSpeed = FMath::Abs(RelativeSpeed);
				if (AbsRelativeSpeed > AbsFastestRelativeTurnSpeed)
				{
					FastestRelativeTurnSampleIdx = SampleIdx;
					OutFastestRelativeTurnSpeed = RelativeSpeed;
					AbsFastestRelativeTurnSpeed = FMath::Abs(OutFastestRelativeTurnSpeed);
				}
			}

			if (AbsFastestRelativeTurnSpeed <= FMath::Abs(TurnSpeedIntervalRadius))
			{
				FastestRelativeTurnSampleIdx = INDEX_NONE;
			}

			return FastestRelativeTurnSampleIdx;
		}

	};
} // namespace UE::MotionTrajectory


bool UMotionTrajectoryBlueprintLibrary::IsSharpVelocityDirChange(
	const FTrajectorySampleRange& Trajectory,
	float MinSharpTurnAngleDegrees,
	ETrajectorySampleDomain RotationConstraintDomain,
	float RotationConstraintValue,
	float MaxAlignmentAngleDegrees,
	float MinLinearSpeed,
	FVector TurnAxis,
	FVector ForwardAxis)
{
	if (Trajectory.Samples.Num() < 2)
	{
		// not enough samples to evaluate the trajectory
		return false;
	}
	const float SquaredMinLinearSpeed = MinLinearSpeed * MinLinearSpeed;
	const int32 LastSampleIdx = Trajectory.Samples.Num() - 1;

	int32 PresentIdx = 0;
	const FTrajectorySample& PresentSample = FTrajectorySampleRange::IterSampleTrajectory(
		Trajectory.Samples,
		ETrajectorySampleDomain::Time,
		0.0f,
		PresentIdx);

	if (PresentIdx >= LastSampleIdx)
	{
		// we need at least one more sample in the future
		return false;
	}

	const FTrajectorySample& LastSample = Trajectory.Samples[LastSampleIdx];
	const float SquaredLastSampleSpeed = LastSample.LinearVelocity.SquaredLength();

	const FTrajectorySample& FirstSample = Trajectory.Samples[0];
	const float SquaredFirstSampleSpeed = FirstSample.LinearVelocity.SquaredLength();

	if (SquaredLastSampleSpeed < SquaredMinLinearSpeed || SquaredFirstSampleSpeed < SquaredMinLinearSpeed)
	{
		// trajectory end points not fast enough
		return false;
	}

	const FTrajectorySample& BeforeLastSample = Trajectory.Samples[LastSampleIdx - 1];

	const float LastDeltaSeconds = LastSample.AccumulatedSeconds - BeforeLastSample.AccumulatedSeconds;
	if (LastDeltaSeconds < SMALL_NUMBER)
	{
		// delta too small to evaluate
		return false;
	}

	UE::MotionTrajectory::FTurnEvaluator TurnEvaluator = UE::MotionTrajectory::FTurnEvaluator(
		Trajectory, 
		RotationConstraintDomain, 
		RotationConstraintValue, 
		TurnAxis);

	if (!TurnEvaluator.IsTurning(PresentIdx, ForwardAxis, MaxAlignmentAngleDegrees))
	{
		return false;
	}

	const bool bIsSharpTurn = TurnEvaluator.FindTurnBeyondExtrapolation(MinSharpTurnAngleDegrees);
	return bIsSharpTurn;
}
