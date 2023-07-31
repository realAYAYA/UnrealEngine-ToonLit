// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionTrajectoryCharacterMovement.h"

#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "KismetAnimationLibrary.h"

FTrajectorySample UCharacterMovementTrajectoryComponent::CalcWorldSpacePresentTrajectorySample(float DeltaTime) const
{
	FTrajectorySample ReturnValue;
	
	const APawn* Pawn = TryGetOwnerPawn();
	if (!Pawn)
	{
		return ReturnValue;
	}

	const UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
	if (MovementComponent)
	{
		if (MovementComponent->MovementMode == EMovementMode::MOVE_Walking)
		{
			FTransform ComponentWorldTransform = Pawn->GetActorTransform();

			ReturnValue.Transform = ComponentWorldTransform;
			ReturnValue.LinearVelocity = MovementComponent->Velocity;
		}
	}
	else
	{
		UE_LOG(
			LogMotionTrajectory,
			Error,
			TEXT("UCharacterMovementTrajectoryComponent expects the owner to have a CharacterMovementComponent"));
	}

	return ReturnValue;
}

UCharacterMovementTrajectoryComponent::UCharacterMovementTrajectoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
}

void UCharacterMovementTrajectoryComponent::InitializeComponent()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.AddDynamic(
			this, 
			&UCharacterMovementTrajectoryComponent::OnMovementUpdated);
		PrimaryComponentTick.bCanEverTick = false;
		PrimaryComponentTick.bStartWithTickEnabled = false;
	}
	else
	{
		UE_LOG(
			LogMotionTrajectory, 
			Error, 
			TEXT("UCharacterMovementTrajectoryComponent expects the owner to be ACharacter"));
	}


	Super::InitializeComponent();
}

void UCharacterMovementTrajectoryComponent::UninitializeComponent()
{
	UCharacterMovementComponent* CharacterMovementComponent = GetOwner()->FindComponentByClass<UCharacterMovementComponent>();
	if (CharacterMovementComponent)
	{
		if (ACharacter* Character = CharacterMovementComponent->GetCharacterOwner())
		{
			Character->OnCharacterMovementUpdated.RemoveDynamic(
				this,
				&UCharacterMovementTrajectoryComponent::OnMovementUpdated);
		}
	}
	else
	{
		UE_LOG(
			LogMotionTrajectory, 
			Error, 
			TEXT("UCharacterMovementTrajectoryComponent expects the owner to have a CharacterMovementComponent"));
	}

	Super::UninitializeComponent();
}

void UCharacterMovementTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();

	const APawn* Pawn = TryGetOwnerPawn();
	if (Pawn && Pawn->Controller)
	{
		LastDesiredControlRotation = Pawn->Controller->GetDesiredRotation();
	}
	else
	{
		LastDesiredControlRotation = FRotator::ZeroRotator;
	}

	DesiredControlRotationVelocity = FRotator::ZeroRotator;
}

void UCharacterMovementTrajectoryComponent::TickTrajectory(float DeltaTime)
{
	Super::TickTrajectory(DeltaTime);
	if (DeltaTime > SMALL_NUMBER)
	{
		const APawn* Pawn = TryGetOwnerPawn();
		if (Pawn && Pawn->Controller)
		{

			const FRotator CurrentDesiredRotation = Pawn->Controller->GetDesiredRotation();

			DesiredControlRotationVelocity = 
				(CurrentDesiredRotation - LastDesiredControlRotation).GetNormalized() * (1.0f / DeltaTime);

			LastDesiredControlRotation = CurrentDesiredRotation;
		}
	}
}

void UCharacterMovementTrajectoryComponent::OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	TickTrajectory(DeltaSeconds);
}

FTrajectorySampleRange UCharacterMovementTrajectoryComponent::GetTrajectory() const
{
	return GetTrajectoryWithSettings(PredictionSettings, bPredictionIncludesHistory);
}

FTrajectorySampleRange UCharacterMovementTrajectoryComponent::GetTrajectoryWithSettings(const FMotionTrajectorySettings& Settings
	, bool bIncludeHistory) const
{
	const APawn* Pawn = TryGetOwnerPawn();
	if (!Pawn)
	{
		return FTrajectorySampleRange(SampleRate);
	}

	const UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());

	// Currently the trajectory prediction only supports the walking movement mode of the character movement component
	if (ensure(MovementComponent) && MovementComponent->MovementMode == EMovementMode::MOVE_Walking)
	{
		// Step the prediction iteratively towards the specified domain horizon(s)
		FTrajectorySampleRange Prediction(SampleRate);
		PredictTrajectory(
			SampleRate, 
			MaxSamples, 
			Settings, 
			PresentTrajectorySampleLS, 
			DesiredControlRotationVelocity,
			Prediction);

		// Combine past, present, and future into a uniformly sampled complete trajectory
		return CombineHistoryPresentPrediction(bIncludeHistory, Prediction);
	}
	else
	{
		return FTrajectorySampleRange(SampleRate);
	}
}

void UCharacterMovementTrajectoryComponent::PredictTrajectory(
	int32 InSampleRate,
	int32 InMaxSamples,
	const FMotionTrajectorySettings& Settings,
	const FTrajectorySample& PresentTrajectory,
	const FRotator& InDesiredControlRotationVelocity,
	FTrajectorySampleRange& OutTrajectoryRange) const
{
	OutTrajectoryRange.SampleRate = InSampleRate;
	const float IntegrationDelta = 1.f / static_cast<float>(InSampleRate);

	if (!!Settings.Domain)
	{
		FTrajectorySample Sample = PresentTrajectory;
		FTrajectorySample PreviousSample = PresentTrajectory;
		float AccumulatedDistance = 0.f;
		float AccumulatedSeconds = 0.f;
		FRotator ControlRotationTotalDelta = FRotator::ZeroRotator;

		constexpr int32 DistanceDomainMask = static_cast<int32>(ETrajectorySampleDomain::Distance);
		constexpr int32 TimeDomainMask = static_cast<int32>(ETrajectorySampleDomain::Time);

		for (int32 Step = 0; Step < InMaxSamples; ++Step)
		{
			PreviousSample = Sample;
			StepPrediction(
				IntegrationDelta, 
				InDesiredControlRotationVelocity, 
				ControlRotationTotalDelta, 
				Sample);

			AccumulatedDistance += 
				FVector::Distance(PreviousSample.Transform.GetLocation(), Sample.Transform.GetLocation());
			Sample.AccumulatedDistance = AccumulatedDistance;
			AccumulatedSeconds += IntegrationDelta;
			Sample.AccumulatedSeconds = AccumulatedSeconds;

			OutTrajectoryRange.Samples.Add(Sample);

			if (FMath::IsNearlyEqual(FMath::Abs(Sample.AccumulatedDistance - PreviousSample.AccumulatedDistance), SMALL_NUMBER) &&
				Sample.Transform.RotationEquals(PreviousSample.Transform, SMALL_NUMBER))
			{
				break;
			}

			if (((Settings.Domain & DistanceDomainMask) == DistanceDomainMask)
				&& (Settings.Distance > 0.f)
				&& (Sample.AccumulatedDistance < Settings.Distance))
			{
				continue;
			}

			if (((Settings.Domain & TimeDomainMask) == TimeDomainMask)
				&& (Settings.Seconds > 0.f)
				&& (Step * IntegrationDelta < Settings.Seconds))
			{
				continue;
			}

			break;
		}
	}
}


// ----------- BEGIN Derived from FCharacterMovementComponentAsyncInput ----------- //
// FCharacterMovementComponentAsyncInput::CalcVelocity() for linear motion
// FCharacterMovementComponentAsyncInput::PhysicsRotation() for angular motion

static FRotator AdjustRotationVerticality(
	const FRotator& InRotation,
	const UCharacterMovementComponent* MovementComponent)
{
	FRotator OutRotation = InRotation;

	if (MovementComponent->ShouldRemainVertical())
	{
		OutRotation.Pitch = 0.f;
		OutRotation.Yaw = FRotator::NormalizeAxis(OutRotation.Yaw);
		OutRotation.Roll = 0.f;
	}
	else
	{
		OutRotation.Normalize();
	}

	return OutRotation;
}

static FRotator LimitRotationRate(
	const FRotator& InRotation,
	const FRotator& CurrentRotation,
	const FRotator& DeltaRot,
	const UCharacterMovementComponent* MovementComponent)
{
	FRotator OutRotation = InRotation;

	// Accumulate a desired new rotation.
	const float AngleTolerance = 1e-3f;

	if (!CurrentRotation.Equals(OutRotation, AngleTolerance))
	{
		// PITCH
		if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, OutRotation.Pitch, AngleTolerance))
		{
			OutRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, OutRotation.Pitch, DeltaRot.Pitch);
		}

		// YAW
		if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, OutRotation.Yaw, AngleTolerance))
		{
			OutRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, OutRotation.Yaw, DeltaRot.Yaw);
		}

		// ROLL
		if (!FMath::IsNearlyEqual(CurrentRotation.Roll, OutRotation.Roll, AngleTolerance))
		{
			OutRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, OutRotation.Roll, DeltaRot.Roll);
		}

		// Set the new rotation.
		OutRotation.DiagnosticCheckNaN(TEXT("LimitRotationRate: OutRotation"));
	}

	return OutRotation;
}

void UCharacterMovementTrajectoryComponent::StepPrediction(
	float IntegrationDelta,
	const FRotator& ControlRotationVelocity,
	FRotator& InOutControlRotationTotalDelta,
	FTrajectorySample& InOutSample) const
{
	const UCharacterMovementComponent* MovementComponent =
		Cast<UCharacterMovementComponent>(TryGetOwnerPawn()->GetMovementComponent());
	if (!IsValid(MovementComponent))
	{
		return;
	}

	const FTransform InitialSampleTransform = InOutSample.Transform;
	const FTransform PrevTransformWS = InitialSampleTransform * MovementComponent->GetOwner()->GetActorTransform();
	
	FRotator ControlRotationDelta = ControlRotationVelocity * IntegrationDelta;
	ControlRotationDelta = AdjustRotationVerticality(ControlRotationDelta, MovementComponent);
	InOutControlRotationTotalDelta += ControlRotationDelta;

	if (MovementComponent->GetCurrentAcceleration().IsZero())
	{
		float ActualBrakingFriction = 
			MovementComponent->bUseSeparateBrakingFriction ? 
			MovementComponent->BrakingFriction : 
			GetFriction(MovementComponent, InOutSample, IntegrationDelta);

		if (!InOutSample.LinearVelocity.IsZero())
		{
			const float FrictionFactor = FMath::Max(0.f, MovementComponent->BrakingFrictionFactor);
			ActualBrakingFriction = FMath::Max(0.f, ActualBrakingFriction * FrictionFactor);
				
			const float BrakingDeceleration = 
				FMath::Max(0.f, GetMaxBrakingDeceleration(MovementComponent, InOutSample, IntegrationDelta));
			const bool bZeroFriction = (ActualBrakingFriction == 0.f);
			const bool bZeroBraking = (BrakingDeceleration == 0.f);

			if (bZeroFriction && bZeroBraking)
			{
				return;
			}

			float RemainingTime = IntegrationDelta;
			const float MaxTimeStep = FMath::Clamp(MovementComponent->BrakingSubStepTime, 1.f / 75.f, 1.f / 20.f);

			const FVector PrevLinearVelocity = InOutSample.LinearVelocity;
			const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * InOutSample.LinearVelocity.GetSafeNormal()));

			while (RemainingTime >= UCharacterMovementComponent::MIN_TICK_TIME)
			{
				const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
				RemainingTime -= dt;

				InOutSample.LinearVelocity = InOutSample.LinearVelocity + ((-ActualBrakingFriction) * InOutSample.LinearVelocity + RevAccel) * dt;

				if ((InOutSample.LinearVelocity | PrevLinearVelocity) <= 0.f)
				{
					InOutSample.LinearVelocity = FVector::ZeroVector;
					return;
				}
			}

			// Clamp to zero if nearly zero, or if below min threshold and braking
			const float VSizeSq = InOutSample.LinearVelocity.SizeSquared();
			if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(UCharacterMovementComponent::BRAKE_TO_STOP_VELOCITY)))
			{
				InOutSample.LinearVelocity = FVector::ZeroVector;
			}
		}
	}
	else
	{
		const FVector LinearAccelerationWS = GetAccelerationWS(MovementComponent, InOutSample, IntegrationDelta);
		const FVector LinearAccelerationAS = 
			TryGetOwnerPawn()->GetActorTransform().Inverse().TransformVectorNoScale(LinearAccelerationWS);

		const FVector RotatedLinearAccelerationAS = InOutControlRotationTotalDelta.RotateVector(LinearAccelerationAS);

		const FVector AccelDir = RotatedLinearAccelerationAS.GetSafeNormal();
		const float VelSize = InOutSample.LinearVelocity.Size();

		InOutSample.LinearVelocity = 
			InOutSample.LinearVelocity - 
			(InOutSample.LinearVelocity - AccelDir * VelSize) * 
			FMath::Min(IntegrationDelta * GetFriction(MovementComponent, InOutSample, IntegrationDelta), 1.f);

		const float MaxInputSpeed = FMath::Max(
			MovementComponent->GetMaxSpeed() * MovementComponent->GetAnalogInputModifier(), 
			MovementComponent->GetMinAnalogSpeed());
		InOutSample.LinearVelocity += RotatedLinearAccelerationAS * IntegrationDelta;
		InOutSample.LinearVelocity = InOutSample.LinearVelocity.GetClampedToMaxSize(MaxInputSpeed);
	}

	{
		const FRotator PrevRotator = PrevTransformWS.GetRotation().Rotator() - InOutControlRotationTotalDelta;
		PrevRotator.DiagnosticCheckNaN(TEXT("StepPrediction: PrevRotator"));

		const FRotator TotalRotation = StepRotationWS(
			MovementComponent, 
			InOutSample, 
			PrevRotator,
			InOutControlRotationTotalDelta,
			IntegrationDelta);

		const FQuat TotalRotationQuat = TotalRotation.Quaternion();
		const FQuat TotalRotationQuatQuatAS = 
			MovementComponent->GetOwner()->GetActorQuat().Inverse() * TotalRotationQuat;

		InOutSample.Transform.SetRotation(TotalRotationQuatQuatAS);
	
		const FVector Translation = InOutSample.LinearVelocity * IntegrationDelta;
		InOutSample.Transform.AddToTranslation(Translation);
	}
}

float UCharacterMovementTrajectoryComponent::GetFriction(
	const UCharacterMovementComponent* MoveComponent, 
	const FTrajectorySample& Sample, 
	float DeltaSeconds) const
{
	return MoveComponent->GroundFriction;
}

float UCharacterMovementTrajectoryComponent::GetMaxBrakingDeceleration(
	const UCharacterMovementComponent* MoveComponent,
	const FTrajectorySample& Sample, 
	float DeltaSeconds) const
{
	return MoveComponent->GetMaxBrakingDeceleration();
}

FVector UCharacterMovementTrajectoryComponent::GetAccelerationWS(
	const UCharacterMovementComponent* MoveComponent,
	const FTrajectorySample& Sample, 
	float DeltaSeconds) const
{
	return MoveComponent->GetCurrentAcceleration();
}

FRotator UCharacterMovementTrajectoryComponent::StepRotationWS(
	const UCharacterMovementComponent* MovementComponent,
	const FTrajectorySample& Sample,
	const FRotator& PrevRotator,
	const FRotator& ControlRotationTotalDelta,
	float IntegrationDelta) const
{
	FRotator BaseRotation = FRotator::ZeroRotator;
	if (MovementComponent->bOrientRotationToMovement)
	{
		FRotator DeltaRot = MovementComponent->GetDeltaRotation(IntegrationDelta);
		DeltaRot.DiagnosticCheckNaN(TEXT("StepPrediction: MovementComponent->GetDeltaRotation"));

		BaseRotation =
			MovementComponent->ComputeOrientToMovementRotation(PrevRotator, IntegrationDelta, DeltaRot);
		BaseRotation = AdjustRotationVerticality(BaseRotation, MovementComponent);
		BaseRotation = LimitRotationRate(BaseRotation, PrevRotator, DeltaRot, MovementComponent);
	}
	else
	{
		BaseRotation = MovementComponent->GetOwner()->GetActorTransform().Rotator();
	}

	const FRotator TotalRotation = BaseRotation + ControlRotationTotalDelta;
	return TotalRotation;
}



// ------------ END Derived from FCharacterMovementComponentAsyncInput ------------ //
