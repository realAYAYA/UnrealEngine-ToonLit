// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterTrajectoryComponent.h"

#include "CharacterMovementTrajectoryLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "MotionTrajectory.h"

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarCharacterTrajectoryDebug(TEXT("a.CharacterTrajectory.Debug"), 0, TEXT("Turn on debug drawing for Character trajectory"));
#endif // ENABLE_ANIM_DEBUG

UCharacterTrajectoryComponent::UCharacterTrajectoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bWantsInitializeComponent = true;
}

void UCharacterTrajectoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (bAutoUpdateTrajectory)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->OnCharacterMovementUpdated.AddDynamic(this, &UCharacterTrajectoryComponent::OnMovementUpdated);
		}
		else
		{
			UE_LOG(LogMotionTrajectory, Error, TEXT("UCharacterTrajectoryComponent requires its owner to be ACharacter"));
		}
	}
}

void UCharacterTrajectoryComponent::UninitializeComponent()
{
	if (bAutoUpdateTrajectory)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->OnCharacterMovementUpdated.RemoveDynamic(this, &UCharacterTrajectoryComponent::OnMovementUpdated);
		}
		else
		{
			UE_LOG(LogMotionTrajectory, Error, TEXT("UCharacterTrajectoryComponent requires its owner to be ACharacter"));
		}
	}

	Super::UninitializeComponent();
}

FRotator UCharacterTrajectoryComponent::GetFacingFromMeshComponent_Implementation(const USkeletalMeshComponent* MeshComponent) const
{
	check(MeshComponent);
	return MeshComponent->GetComponentRotation();
}

void UCharacterTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!ensureMsgf(Character, TEXT("UCharacterTrajectoryComponent requires valid ACharacter owner.")))
	{
		return;
	}

	SkelMeshComponent = Character->GetMesh();
	if (!ensureMsgf(SkelMeshComponent, TEXT("UCharacterTrajectoryComponent must be run on an ACharacter with a valid USkeletalMeshComponent.")))
	{
		return;
	}

	CharacterMovementComponent = Character->GetCharacterMovement();
	if (!ensureMsgf(CharacterMovementComponent, TEXT("UCharacterTrajectoryComponent must be run on an ACharacter with a valid UCharacterMovementComponent.")))
	{
		return;
	}

	// Default forward in the engine is the X axis, but data often diverges from this (e.g. it's common for skeletal meshes to be Y forward).
	// We determine the forward direction in the space of the skeletal mesh component based on the offset from the actor.
	ForwardFacingCS = SkelMeshComponent->GetRelativeRotation().Quaternion().Inverse();

	// The UI clamps these to be non-zero.
	check(HistorySamplesPerSecond);
	check(PredictionSamplesPerSecond);

	NumHistorySamples = FMath::CeilToInt32(HistoryLengthSeconds * HistorySamplesPerSecond);
	SecondsPerHistorySample = 1.f / HistorySamplesPerSecond;

	const int32 NumPredictionSamples = FMath::CeilToInt32(PredictionLengthSeconds * PredictionSamplesPerSecond);
	SecondsPerPredictionSample = 1.f / PredictionSamplesPerSecond;

	FPoseSearchQueryTrajectorySample DefaultSample;
	DefaultSample.Facing = ForwardFacingCS;
	DefaultSample.Position = FVector::ZeroVector;
	DefaultSample.AccumulatedSeconds = 0.f;

	const FVector SkelMeshComponentLocationWS = SkelMeshComponent->GetComponentLocation();
	const FQuat FacingWS = FQuat(GetFacingFromMeshComponent(SkelMeshComponent));

	// History + current sample + prediction
	Trajectory.Samples.Init(DefaultSample, NumHistorySamples + 1 + NumPredictionSamples);

	// initializing history samples AccumulatedSeconds
	for (int32 i = 0; i < NumHistorySamples; ++i)
	{
		Trajectory.Samples[i].AccumulatedSeconds = SecondsPerHistorySample * (i - NumHistorySamples);
		Trajectory.Samples[i].Position = SkelMeshComponentLocationWS;
		Trajectory.Samples[i].Facing = FacingWS;
	}

	// initializing history samples AccumulatedSeconds
	for (int32 i = NumHistorySamples + 1; i < Trajectory.Samples.Num(); ++i)
	{
		Trajectory.Samples[i].AccumulatedSeconds = SecondsPerPredictionSample * (i - NumHistorySamples);
		Trajectory.Samples[i].Position = SkelMeshComponentLocationWS;
		Trajectory.Samples[i].Facing = FacingWS;
	}
}

void UCharacterTrajectoryComponent::UpdateTrajectory(float DeltaSeconds)
{
	if (!ensure(CharacterMovementComponent != nullptr && SkelMeshComponent != nullptr))
	{
		return;
	}

	if (!ensure(DeltaSeconds > 0.f))
	{
		return;
	}

	const FVector SkelMeshComponentLocationWS = SkelMeshComponent->GetComponentLocation();
	const FQuat FacingWS = FQuat(GetFacingFromMeshComponent(SkelMeshComponent));
	const FRotator ControllerRotationRate = CalculateControllerRotationRate(DeltaSeconds, CharacterMovementComponent->ShouldRemainVertical());
		
	UpdateHistory(DeltaSeconds);
	UpdatePrediction(SkelMeshComponentLocationWS, FacingWS, CharacterMovementComponent->Velocity, CharacterMovementComponent->GetCurrentAcceleration(), ControllerRotationRate);

#if ENABLE_ANIM_DEBUG
	if (CVarCharacterTrajectoryDebug.GetValueOnAnyThread())
	{
		Trajectory.DebugDrawTrajectory(GetWorld());
	}
#endif // ENABLE_ANIM_DEBUG
}

void UCharacterTrajectoryComponent::OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	UpdateTrajectory(DeltaSeconds);
}

// This function shifts the range of history samples whenever a new history sample should be recorded.
// This allows us to keep a single sample array in world space that can be read directly by the Motion Matching node. 
void UCharacterTrajectoryComponent::UpdateHistory(float DeltaSeconds)
{
	check(NumHistorySamples <= Trajectory.Samples.Num());

	// Shift history Samples when it's time to record a new one.
	if (NumHistorySamples > 0 && FMath::Abs(Trajectory.Samples[NumHistorySamples - 1].AccumulatedSeconds) >= SecondsPerHistorySample)
	{
		for (int32 Index = 0; Index < NumHistorySamples; ++Index)
		{
			Trajectory.Samples[Index] = Trajectory.Samples[Index + 1];
			Trajectory.Samples[Index].AccumulatedSeconds -= DeltaSeconds;
		}
	}
	else
	{
		for (int32 Index = 0; Index < NumHistorySamples; ++Index)
		{
			Trajectory.Samples[Index].AccumulatedSeconds -= DeltaSeconds;
		}
	}
}

void UCharacterTrajectoryComponent::UpdatePrediction(const FVector& PositionWS, const FQuat& FacingWS, const FVector& VelocityWS, const FVector& AccelerationWS, const FRotator& ControllerRotationRate)
{
	check(CharacterMovementComponent);

	FVector CurrentPositionWS = PositionWS;
	FVector CurrentVelocityWS = VelocityWS;
	FVector CurrentAccelerationWS = AccelerationWS;
	FQuat CurrentFacingWS = FacingWS;
	float AccumulatedSeconds = 0.f;

	FQuat ControllerRotationPerStep = (ControllerRotationRate * SecondsPerPredictionSample).Quaternion();

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

			// Account for the controller (e.g. the camera) rotating.
			CurrentFacingWS = ControllerRotationPerStep * CurrentFacingWS;
			CurrentAccelerationWS = ControllerRotationPerStep * CurrentAccelerationWS;

			FVector NewVelocityCS = FVector::ZeroVector;
			UCharacterMovementTrajectoryLibrary::StepCharacterMovementGroundPrediction(SecondsPerPredictionSample, CurrentVelocityWS, CurrentAccelerationWS, CharacterMovementComponent, NewVelocityCS);
			CurrentVelocityWS = NewVelocityCS;

			if (CharacterMovementComponent->bOrientRotationToMovement && !CurrentAccelerationWS.IsNearlyZero())
			{
				// Rotate towards acceleration.
				const FVector CurrentAccelerationCS = SkelMeshComponent->GetRelativeRotation().Quaternion().RotateVector(CurrentAccelerationWS);
				CurrentFacingWS = FMath::QInterpConstantTo(CurrentFacingWS, CurrentAccelerationCS.ToOrientationQuat(), SecondsPerPredictionSample, RotateTowardsMovementSpeed);
			}
		}
	}
}

// Calculate how much the character is rotating each update due to the controller (e.g. the camera) rotating.
// E.g. If the user is moving forward but rotating the camera, the character (and thus future accelerations, facing directions, etc) will rotate.
FRotator UCharacterTrajectoryComponent::CalculateControllerRotationRate(float DeltaSeconds, bool bShouldRemainVertical)
{
	check(GetOwner());

	// UpdateTrajectory handles DeltaSeconds == 0.f, so we should never hit this.
	check(DeltaSeconds > 0.f);

	ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	if (CharacterOwner == nullptr || CharacterOwner->Controller == nullptr)
	{
		// @todo: Simulated proxies don't have controllers, so they'll need some other mechanism to account for controller rotation rate.
		return FRotator::ZeroRotator;
	}

	FRotator DesiredControllerRotation = CharacterOwner->Controller->GetDesiredRotation();
	if (bShouldRemainVertical)
	{
		DesiredControllerRotation.Yaw = FRotator::NormalizeAxis(DesiredControllerRotation.Yaw);
		DesiredControllerRotation.Pitch = 0.f;
		DesiredControllerRotation.Roll = 0.f;
	}

	const FRotator DesiredRotationDelta = DesiredControllerRotation - DesiredControllerRotationLastUpdate;
	DesiredControllerRotationLastUpdate = DesiredControllerRotation;

	FRotator ControllerRotationRate = DesiredRotationDelta.GetNormalized() * (1.f / DeltaSeconds);
	if (MaxControllerRotationRate >= 0.f)
	{
		ControllerRotationRate.Pitch = FMath::Sign(ControllerRotationRate.Pitch) * FMath::Min(FMath::Abs(ControllerRotationRate.Pitch), MaxControllerRotationRate);
		ControllerRotationRate.Yaw = FMath::Sign(ControllerRotationRate.Yaw) * FMath::Min(FMath::Abs(ControllerRotationRate.Yaw), MaxControllerRotationRate);
		ControllerRotationRate.Roll = FMath::Sign(ControllerRotationRate.Roll) * FMath::Min(FMath::Abs(ControllerRotationRate.Roll), MaxControllerRotationRate);
	}

	return ControllerRotationRate;
}