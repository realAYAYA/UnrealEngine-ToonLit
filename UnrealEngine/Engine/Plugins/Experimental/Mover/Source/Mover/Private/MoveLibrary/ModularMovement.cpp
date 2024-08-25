// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/ModularMovement.h"
#include "MoverTypes.h"
#include "MoverSimulationTypes.h"


FRotator ULinearTurnGenerator::GetTurn_Implementation(FRotator TargetOrientation, const FMoverTickStartData& FullStartState, const FMoverDefaultSyncState& MoverState, const FMoverTimeStep& TimeStep, const FProposedMove& ProposedMove, UMoverBlackboard* SimBlackboard)
{
	FRotator AngularVelocityDpS(FRotator::ZeroRotator);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	if (DeltaSeconds > 0.f)
	{
		FRotator AngularDelta = (TargetOrientation - MoverState.GetOrientation_WorldSpace());
		FRotator Winding, Remainder;

		AngularDelta.GetWindingAndRemainder(Winding, Remainder);	// to find the fastest turn, just get the (-180,180] remainder

		AngularVelocityDpS = Remainder * (1.f / DeltaSeconds);

		FRotator TurnRates(PitchRate, HeadingRate, RollRate);

		if (HeadingRate >= 0.f)
		{
			AngularVelocityDpS.Yaw = FMath::Clamp(AngularVelocityDpS.Yaw, -HeadingRate, HeadingRate);
		}

		if (PitchRate >= 0.f)
		{
			AngularVelocityDpS.Pitch = FMath::Clamp(AngularVelocityDpS.Pitch, -PitchRate, PitchRate);
		}

		if (RollRate >= 0.f)
		{
			AngularVelocityDpS.Roll = FMath::Clamp(AngularVelocityDpS.Roll, -RollRate, RollRate);
		}
	}

	return AngularVelocityDpS;
}


// Note the lack of argument range checking.  Value and Time arguments can be in any units, as long as they're consistent.
static float CalcExactDampedInterpolation(float CurrentVal, float TargetVal, float HalflifeTime, float DeltaTime)
{
	return FMath::Lerp(CurrentVal, TargetVal, 1.f - FMath::Pow(2.f, (-DeltaTime / HalflifeTime)));
}

FRotator UExactDampedTurnGenerator::GetTurn_Implementation(FRotator TargetOrientation, const FMoverTickStartData& FullStartState, const FMoverDefaultSyncState& MoverState, const FMoverTimeStep& TimeStep, const FProposedMove& ProposedMove, UMoverBlackboard* SimBlackboard)
{
	FRotator AngularVelocityDpS(FRotator::ZeroRotator);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	if (DeltaSeconds > 0.f && HalfLifeSeconds > UE_KINDA_SMALL_NUMBER)
	{
		FRotator AngularDelta = (TargetOrientation - MoverState.GetOrientation_WorldSpace());
		FRotator Winding, Remainder;

		AngularDelta.GetWindingAndRemainder(Winding, Remainder);	// to find the fastest turn, just get the (-180,180] remainder

		const float OneOverDeltaSecs = 1.f / DeltaSeconds;

		AngularVelocityDpS.Yaw   = CalcExactDampedInterpolation(0.f, Remainder.Yaw,   HalfLifeSeconds, DeltaSeconds) * OneOverDeltaSecs;
		AngularVelocityDpS.Pitch = CalcExactDampedInterpolation(0.f, Remainder.Pitch, HalfLifeSeconds, DeltaSeconds) * OneOverDeltaSecs;
		AngularVelocityDpS.Roll  = CalcExactDampedInterpolation(0.f, Remainder.Roll,  HalfLifeSeconds, DeltaSeconds) * OneOverDeltaSecs;
	}

	return AngularVelocityDpS;
}