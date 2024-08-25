// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/MovementRecord.h"
#include "MoverLog.h"

void FMovementRecord::Append(FMovementSubstep Substep)
{
	if (bIsRelevancyLocked)
	{
		Substep.bIsRelevant = bRelevancyLockValue;
	}

	if (Substep.bIsRelevant)
	{
		RelevantMoveDelta += Substep.MoveDelta;
	}

	TotalMoveDelta += Substep.MoveDelta;

	Substeps.Add(Substep);
}

FString FMovementRecord::ToString() const
{
	return FString::Printf( TEXT("TotalMove: %s over %.3f seconds. RelevantVelocity: %s. Substeps: %s"),
		*TotalMoveDelta.ToCompactString(),
		TotalDeltaSeconds,
		*GetRelevantVelocity().ToCompactString(),
		*FString::JoinBy(Substeps, TEXT(","), [](const FMovementSubstep& Substep) { return Substep.MoveName.ToString(); }));
}
