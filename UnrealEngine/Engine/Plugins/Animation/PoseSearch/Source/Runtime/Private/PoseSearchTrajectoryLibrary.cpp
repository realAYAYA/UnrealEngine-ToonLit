// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void FPoseSearchTrajectoryData::UpdateData(
	float DeltaTime,
	const FAnimInstanceProxy& AnimInstanceProxy,
	FDerived& TrajectoryDataDerived,
	FState& TrajectoryDataState) const
{
	UpdateData(DeltaTime, Cast<const UAnimInstance>(AnimInstanceProxy.GetAnimInstanceObject()), TrajectoryDataDerived, TrajectoryDataState);
}

void FPoseSearchTrajectoryData::UpdateData(
	float DeltaTime,
	const UAnimInstance* AnimInstance,
	FDerived& TrajectoryDataDerived,
	FState& TrajectoryDataState) const
{
	// An AnimInstance might call this during an AnimBP recompile with 0 delta time.
	if (DeltaTime <= 0.f || !AnimInstance)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(AnimInstance->GetOwningActor());
	if (!Character)
	{
		return;
	}

	if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		TrajectoryDataDerived.MaxSpeed = FMath::Max(MoveComp->GetMaxSpeed() * MoveComp->GetAnalogInputModifier(), MoveComp->GetMinAnalogSpeed());
		TrajectoryDataDerived.BrakingDeceleration = FMath::Max(0.f, MoveComp->GetMaxBrakingDeceleration());
		TrajectoryDataDerived.bOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

		TrajectoryDataDerived.Velocity = MoveComp->Velocity;
		TrajectoryDataDerived.Acceleration = MoveComp->GetCurrentAcceleration();
		
		TrajectoryDataDerived.bStepGroundPrediction = !MoveComp->IsFalling() && !MoveComp->IsFlying();

		if (TrajectoryDataDerived.Acceleration.IsZero())
		{
			TrajectoryDataDerived.Friction = MoveComp->bUseSeparateBrakingFriction ? MoveComp->BrakingFriction : MoveComp->GroundFriction;
			const float FrictionFactor = FMath::Max(0.f, MoveComp->BrakingFrictionFactor);
			TrajectoryDataDerived.Friction = FMath::Max(0.f, TrajectoryDataDerived.Friction * FrictionFactor);
		}
		else
		{
			TrajectoryDataDerived.Friction = MoveComp->GroundFriction;
		}
	}

	// @todo: Simulated proxies don't have controllers, so they'll need some other mechanism to account for controller rotation rate.
	const AController* Controller = Character->Controller;
	if (Controller)
	{
		const float DesiredControllerYaw = Controller->GetDesiredRotation().Yaw;
		
		const float DesiredYawDelta = DesiredControllerYaw - TrajectoryDataState.DesiredControllerYawLastUpdate;
		TrajectoryDataState.DesiredControllerYawLastUpdate = DesiredControllerYaw;

		TrajectoryDataDerived.ControllerYawRate = FRotator::NormalizeAxis(DesiredYawDelta) * (1.f / DeltaTime);
		if (MaxControllerYawRate >= 0.f)
		{
			TrajectoryDataDerived.ControllerYawRate = FMath::Sign(TrajectoryDataDerived.ControllerYawRate) * FMath::Min(FMath::Abs(TrajectoryDataDerived.ControllerYawRate), MaxControllerYawRate);
		}
	}

	if (const USkeletalMeshComponent* MeshComp = Character->GetMesh())
	{
		TrajectoryDataDerived.Position = MeshComp->GetComponentLocation();
		TrajectoryDataDerived.Facing = MeshComp->GetComponentRotation().Quaternion();
		TrajectoryDataDerived.MeshCompRelativeRotation = MeshComp->GetRelativeRotation().Quaternion();
	}
}

FVector FPoseSearchTrajectoryData::StepCharacterMovementGroundPrediction(
	float DeltaTime,
	const FVector& InVelocity,
	const FVector& InAcceleration,
	const FDerived& TrajectoryDataDerived) const
{
	FVector OutVelocity = InVelocity;

	// Braking logic is copied from UCharacterMovementComponent::ApplyVelocityBraking()
	if (InAcceleration.IsZero())
	{
		if (InVelocity.IsZero())
		{
			return FVector::ZeroVector;
		}

		const bool bZeroFriction = (TrajectoryDataDerived.Friction == 0.f);
		const bool bZeroBraking = (TrajectoryDataDerived.BrakingDeceleration == 0.f);

		if (bZeroFriction && bZeroBraking)
		{
			return InVelocity;
		}

		static const float MaxTimeStep = 1.f / 60.f;
		float RemainingTime = DeltaTime;

		const FVector PrevLinearVelocity = OutVelocity;
		const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-TrajectoryDataDerived.BrakingDeceleration * OutVelocity.GetSafeNormal()));

		// Decelerate to brake to a stop
		while (RemainingTime >= UCharacterMovementComponent::MIN_TICK_TIME)
		{
			// Zero friction uses constant deceleration, so no need for iteration.
			const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
			RemainingTime -= dt;

			// apply friction and braking
			OutVelocity = OutVelocity + ((-TrajectoryDataDerived.Friction) * OutVelocity + RevAccel) * dt;

			// Don't reverse direction
			if ((OutVelocity | PrevLinearVelocity) <= 0.f)
			{
				OutVelocity = FVector::ZeroVector;
				return OutVelocity;
			}
		}

		// Clamp to zero if nearly zero, or if below min threshold and braking
		const float VSizeSq = OutVelocity.SizeSquared();
		if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(UCharacterMovementComponent::BRAKE_TO_STOP_VELOCITY)))
		{
			OutVelocity = FVector::ZeroVector;
		}
	}
	// Acceleration logic is copied from  UCharacterMovementComponent::CalcVelocity
	else
	{
		const FVector AccelDir = InAcceleration.GetSafeNormal();
		const float VelSize = OutVelocity.Size();

		OutVelocity = OutVelocity - (OutVelocity - AccelDir * VelSize) * FMath::Min(DeltaTime * TrajectoryDataDerived.Friction, 1.f);

		OutVelocity += InAcceleration * DeltaTime;
		OutVelocity = OutVelocity.GetClampedToMaxSize(TrajectoryDataDerived.MaxSpeed);
	}

	return OutVelocity;
}

void UPoseSearchTrajectoryLibrary::InitTrajectorySamples(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,
	const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	const int32 NumPredictionSamples = TrajectoryDataSampling.NumPredictionSamples;

	// History + current sample + prediction
	const int32 TotalNumSamples = NumHistorySamples + 1 + NumPredictionSamples;

	if (Trajectory.Samples.Num() != TotalNumSamples)
	{
		Trajectory.Samples.SetNumUninitialized(TotalNumSamples);

		// Initialize history samples
		const float SecondsPerHistorySample = FMath::Max(TrajectoryDataSampling.SecondsPerHistorySample, 0.f);
		for (int32 i = 0; i < NumHistorySamples; ++i)
		{
			Trajectory.Samples[i].Position = TrajectoryDataDerived.Position;
			Trajectory.Samples[i].Facing = TrajectoryDataDerived.Facing;
			Trajectory.Samples[i].AccumulatedSeconds = SecondsPerHistorySample * (i - NumHistorySamples - 1);
		}

		// Initialize current sample and prediction
		const float SecondsPerPredictionSample = FMath::Max(TrajectoryDataSampling.SecondsPerPredictionSample, 0.f);
		for (int32 i = NumHistorySamples; i < Trajectory.Samples.Num(); ++i)
		{
			Trajectory.Samples[i].Position = TrajectoryDataDerived.Position;
			Trajectory.Samples[i].Facing = TrajectoryDataDerived.Facing;
			Trajectory.Samples[i].AccumulatedSeconds = SecondsPerPredictionSample * (i - NumHistorySamples) + DeltaTime;
		}
	}
}

void UPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,
	const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	if (NumHistorySamples > 0)
	{
		const float SecondsPerHistorySample = TrajectoryDataSampling.SecondsPerHistorySample;

		check(NumHistorySamples <= Trajectory.Samples.Num());

		// converting all the history samples relative to the previous character position (Trajectory.Samples[NumHistorySamples].Position)
		for (int32 Index = 0; Index < NumHistorySamples; ++Index)
		{
			Trajectory.Samples[Index].Position = Trajectory.Samples[NumHistorySamples].Position - Trajectory.Samples[Index].Position;
		}

		FVector CurrentTranslation = TrajectoryDataDerived.Velocity * DeltaTime;

		// Shift history Samples when it's time to record a new one.
		if (SecondsPerHistorySample <= 0.f || FMath::Abs(Trajectory.Samples[NumHistorySamples - 1].AccumulatedSeconds) >= SecondsPerHistorySample)
		{
			for (int32 Index = 0; Index < NumHistorySamples - 1; ++Index)
			{
				Trajectory.Samples[Index].AccumulatedSeconds = Trajectory.Samples[Index + 1].AccumulatedSeconds - DeltaTime;
				Trajectory.Samples[Index].Position = Trajectory.Samples[Index + 1].Position + CurrentTranslation;
				Trajectory.Samples[Index].Facing = Trajectory.Samples[Index + 1].Facing;
			}

			Trajectory.Samples[NumHistorySamples - 1].AccumulatedSeconds = 0.f;
			Trajectory.Samples[NumHistorySamples - 1].Position = CurrentTranslation;
			Trajectory.Samples[NumHistorySamples - 1].Facing = Trajectory.Samples[NumHistorySamples].Facing;
		}
		else
		{
			for (int32 Index = 0; Index < NumHistorySamples; ++Index)
			{
				Trajectory.Samples[Index].AccumulatedSeconds -= DeltaTime;
				Trajectory.Samples[Index].Position += CurrentTranslation;
			}
		}

		// converting the history sample positions in world space by applying the current world position.
		for (int32 Index = 0; Index < NumHistorySamples; ++Index)
		{
			Trajectory.Samples[Index].Position = TrajectoryDataDerived.Position - Trajectory.Samples[Index].Position;
		}
	}
}

FVector UPoseSearchTrajectoryLibrary::RemapVectorMagnitudeWithCurve(
	const FVector& Vector,
	bool bUseCurve,
	const FRuntimeFloatCurve& Curve)
{
	if (bUseCurve)
	{
		const float Length = Vector.Length();
		if (Length > UE_KINDA_SMALL_NUMBER)
		{
			const float RemappedLength = Curve.GetRichCurveConst()->Eval(Length);
			return Vector * (RemappedLength / Length);
		}
	}

	return Vector;
}

void UPoseSearchTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,
	const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	FVector CurrentPositionWS = TrajectoryDataDerived.Position;
	FVector CurrentVelocityWS = RemapVectorMagnitudeWithCurve(TrajectoryDataDerived.Velocity, TrajectoryData.bUseSpeedRemappingCurve, TrajectoryData.SpeedRemappingCurve);
	FVector CurrentAccelerationWS = RemapVectorMagnitudeWithCurve(TrajectoryDataDerived.Acceleration, TrajectoryData.bUseAccelerationRemappingCurve, TrajectoryData.AccelerationRemappingCurve);

	// Bending CurrentVelocityWS towards CurrentAccelerationWS
	if (TrajectoryData.BendVelocityTowardsAcceleration > UE_KINDA_SMALL_NUMBER && !CurrentAccelerationWS.IsNearlyZero())
	{
		const float CurrentSpeed = CurrentVelocityWS.Length();
		const FVector VelocityWSAlongAcceleration = CurrentAccelerationWS.GetUnsafeNormal() * CurrentSpeed;
		if (TrajectoryData.BendVelocityTowardsAcceleration < 1.f - UE_KINDA_SMALL_NUMBER)
		{
			CurrentVelocityWS = FMath::Lerp(CurrentVelocityWS, VelocityWSAlongAcceleration, TrajectoryData.BendVelocityTowardsAcceleration);

			const float NewLength = CurrentVelocityWS.Length();
			if (NewLength > UE_KINDA_SMALL_NUMBER)
			{
				CurrentVelocityWS *= CurrentSpeed / NewLength;
			}
			else
			{
				// @todo: consider setting the CurrentVelocityWS = VelocityWSAlongAcceleration if vel and acc are in opposite directions
			}
		}
		else
		{
			CurrentVelocityWS = VelocityWSAlongAcceleration;
		}
	}

	FQuat CurrentFacingWS = TrajectoryDataDerived.Facing;
	
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	const float SecondsPerPredictionSample = TrajectoryDataSampling.SecondsPerPredictionSample;
	const FQuat ControllerRotationPerStep = FQuat::MakeFromEuler(FVector(0.f, 0.f, TrajectoryDataDerived.ControllerYawRate * SecondsPerPredictionSample));

	float AccumulatedSeconds = DeltaTime;

	const int32 LastIndex = Trajectory.Samples.Num() - 1;
	if (NumHistorySamples <= LastIndex)
	{
		for (int32 Index = NumHistorySamples; ; ++Index)
		{
			Trajectory.Samples[Index].Position = CurrentPositionWS;
			Trajectory.Samples[Index].Facing = CurrentFacingWS;
			Trajectory.Samples[Index].AccumulatedSeconds = AccumulatedSeconds;

			if (Index == LastIndex)
			{
				break;
			}

			CurrentPositionWS += CurrentVelocityWS * SecondsPerPredictionSample;
			AccumulatedSeconds += SecondsPerPredictionSample;

			if (TrajectoryDataDerived.bStepGroundPrediction)
			{
				CurrentAccelerationWS = RemapVectorMagnitudeWithCurve(ControllerRotationPerStep * CurrentAccelerationWS,
					TrajectoryData.bUseAccelerationRemappingCurve, TrajectoryData.AccelerationRemappingCurve);
				const FVector NewVelocityWS = TrajectoryData.StepCharacterMovementGroundPrediction(SecondsPerPredictionSample, CurrentVelocityWS, CurrentAccelerationWS, TrajectoryDataDerived);
				CurrentVelocityWS = RemapVectorMagnitudeWithCurve(NewVelocityWS, TrajectoryData.bUseSpeedRemappingCurve, TrajectoryData.SpeedRemappingCurve);

				// Account for the controller (e.g. the camera) rotating.
				CurrentFacingWS = ControllerRotationPerStep * CurrentFacingWS;
				if (TrajectoryDataDerived.bOrientRotationToMovement && !CurrentAccelerationWS.IsNearlyZero())
				{
					// Rotate towards acceleration.
					const FVector CurrentAccelerationCS = TrajectoryDataDerived.MeshCompRelativeRotation.RotateVector(CurrentAccelerationWS);
					CurrentFacingWS = FMath::QInterpConstantTo(CurrentFacingWS, CurrentAccelerationCS.ToOrientationQuat(), SecondsPerPredictionSample, TrajectoryData.RotateTowardsMovementSpeed);
				}
			}
		}
	}
}

void UPoseSearchTrajectoryLibrary::PoseSearchGenerateTrajectory(
	const UAnimInstance* InAnimInstance, UPARAM(ref)
	const FPoseSearchTrajectoryData& InTrajectoryData,
	float InDeltaTime,
	UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory,
	UPARAM(ref) float& InOutDesiredControllerYawLastUpdate,
	FPoseSearchQueryTrajectory& OutTrajectory,
	float InHistorySamplingInterval,
	int32 InTrajectoryHistoryCount,
	float InPredictionSamplingInterval,
	int32 InTrajectoryPredictionCount)
{
	FPoseSearchTrajectoryData::FSampling TrajectoryDataSampling;
	TrajectoryDataSampling.NumHistorySamples = InTrajectoryHistoryCount;
	TrajectoryDataSampling.SecondsPerHistorySample = InHistorySamplingInterval;
	TrajectoryDataSampling.NumPredictionSamples = InTrajectoryPredictionCount;
	TrajectoryDataSampling.SecondsPerPredictionSample = InPredictionSamplingInterval;

	FPoseSearchTrajectoryData::FState TrajectoryDataState;
	TrajectoryDataState.DesiredControllerYawLastUpdate = InOutDesiredControllerYawLastUpdate;

	FPoseSearchTrajectoryData::FDerived TrajectoryDataDerived;
	InTrajectoryData.UpdateData(InDeltaTime, InAnimInstance, TrajectoryDataDerived, TrajectoryDataState);
	InitTrajectorySamples(InOutTrajectory, InTrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, InDeltaTime);
	UpdateHistory_TransformHistory(InOutTrajectory, InTrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, InDeltaTime);
	UpdatePrediction_SimulateCharacterMovement(InOutTrajectory, InTrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, InDeltaTime);

	InOutDesiredControllerYawLastUpdate = TrajectoryDataState.DesiredControllerYawLastUpdate;

	OutTrajectory = InOutTrajectory;
}

void UPoseSearchTrajectoryLibrary::HandleTrajectoryWorldCollisions(const UObject* WorldContextObject, const UAnimInstance* AnimInstance, UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, bool bApplyGravity, float FloorCollisionsOffset, FPoseSearchQueryTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
	ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf, float MaxObstacleHeight, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	OutTrajectory = InTrajectory;

	TArray<FPoseSearchQueryTrajectorySample>& Samples = OutTrajectory.Samples;
	const int32 NumSamples = Samples.Num();

	FVector GravityDirection = FVector::ZeroVector;
	float GravityZ = 0.f;
	float InitialVelocityZ = 0.0f;
	if (bApplyGravity && AnimInstance)
	{
		if (const ACharacter* Character = Cast<ACharacter>(AnimInstance->GetOwningActor()))
		{
			if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				GravityZ = MoveComp->GetGravityZ();
				GravityDirection = MoveComp->GetGravityDirection();
				InitialVelocityZ = Character->GetVelocity().Z;
			}
		}
	}
	CollisionResult.TimeToLand = OutTrajectory.Samples.Last().AccumulatedSeconds;

	if (!FMath::IsNearlyZero(GravityZ))
	{
		FVector LastImpactPoint;
		FVector LastImpactNormal;
		bool bIsLastImpactValid = false;
		bool bIsFirstFall = true;

		const FVector Gravity = GravityDirection * -GravityZ;
		float FreeFallAccumulatedSeconds = 0.f;
		for (int32 SampleIndex = 1; SampleIndex < NumSamples; ++SampleIndex)
		{
			FPoseSearchQueryTrajectorySample& Sample = Samples[SampleIndex];
			if (Sample.AccumulatedSeconds > 0.f)
			{
				const int32 PrevSampleIndex = SampleIndex - 1;
				const FPoseSearchQueryTrajectorySample& PrevSample = Samples[PrevSampleIndex];

				FreeFallAccumulatedSeconds += Sample.AccumulatedSeconds - PrevSample.AccumulatedSeconds;

				// projecting Sample.Position on the HitResult plane and offsetting it by FloorCollisionsOffset
				if (bIsLastImpactValid)
				{
					const FVector DeltaImpactPoint = Sample.Position - LastImpactPoint;
					Sample.Position += (FloorCollisionsOffset - (DeltaImpactPoint | LastImpactNormal)) * LastImpactNormal;
				}

				// applying gravity
				const FVector FreeFallOffset = Gravity * (0.5f * FreeFallAccumulatedSeconds * FreeFallAccumulatedSeconds);
				Sample.Position += FreeFallOffset;

				FHitResult HitResult;
				if (FloorCollisionsOffset > 0.f && UKismetSystemLibrary::LineTraceSingle(WorldContextObject, Sample.Position + (GravityDirection * -MaxObstacleHeight), Sample.Position, TraceChannel, bTraceComplex, ActorsToIgnore, DrawDebugType, HitResult, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime))
				{
					LastImpactPoint = HitResult.ImpactPoint;
					LastImpactNormal = HitResult.Normal;
					bIsLastImpactValid = true;

					Sample.Position = LastImpactPoint + (LastImpactNormal * FloorCollisionsOffset);

					if (bIsFirstFall)
					{
						const float InitialHeight = OutTrajectory.GetSampleAtTime(0.0f).Position.Z;
						const float FinalHeight = Sample.Position.Z;
						const float FallHeight = FMath::Abs(FinalHeight - InitialHeight);

						bIsFirstFall = false;
						CollisionResult.TimeToLand = (InitialVelocityZ / -GravityZ) + ((FMath::Sqrt(FMath::Square(InitialVelocityZ) + (2.f * -GravityZ * FallHeight))) / -GravityZ);
						CollisionResult.LandSpeed = InitialVelocityZ + GravityZ * CollisionResult.TimeToLand;
					}

					FreeFallAccumulatedSeconds = 0.f;
				}
			}
		}
	}
	else if (FloorCollisionsOffset > 0.f)
	{
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			FPoseSearchQueryTrajectorySample& Sample = OutTrajectory.Samples[SampleIndex];
			if (Sample.AccumulatedSeconds > 0.f)
			{
				FHitResult HitResult;
				if (UKismetSystemLibrary::LineTraceSingle(WorldContextObject, Sample.Position + FVector::UpVector * 3000.f, Sample.Position, TraceChannel, bTraceComplex, ActorsToIgnore, DrawDebugType, HitResult, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime))
				{
					Sample.Position.Z = HitResult.ImpactPoint.Z + FloorCollisionsOffset;
				}
			}
		}
	}

	CollisionResult.LandSpeed = InitialVelocityZ + GravityZ * CollisionResult.TimeToLand;
}

void UPoseSearchTrajectoryLibrary::GetTrajectorySampleAtTime(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time, FPoseSearchQueryTrajectorySample& OutTrajectorySample, bool bExtrapolate)
{
	OutTrajectorySample = InTrajectory.GetSampleAtTime(Time, bExtrapolate);
}

