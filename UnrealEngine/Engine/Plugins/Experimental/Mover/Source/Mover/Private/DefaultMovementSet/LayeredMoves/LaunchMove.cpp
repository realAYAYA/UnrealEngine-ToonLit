// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "MoverLog.h"


FLayeredMove_Launch::FLayeredMove_Launch()
{
	DurationMs = 0.f;
	MixMode = EMoveMixMode::OverrideVelocity;
}

bool FLayeredMove_Launch::GenerateMove(const FMoverTickStartData& SimState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	OutProposedMove.MixMode = MixMode;
	OutProposedMove.LinearVelocity = LaunchVelocity;
	OutProposedMove.PreferredMode = ForceMovementMode;

	return true;
}

FLayeredMoveBase* FLayeredMove_Launch::Clone() const
{
	FLayeredMove_Launch* CopyPtr = new FLayeredMove_Launch(*this);
	return CopyPtr;
}

void FLayeredMove_Launch::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 16>(LaunchVelocity, Ar);

	bool bUsingForcedMovementMode = !ForceMovementMode.IsNone();

	Ar.SerializeBits(&bUsingForcedMovementMode, 1);

	if (bUsingForcedMovementMode)
	{
		Ar << ForceMovementMode;
	}

}

UScriptStruct* FLayeredMove_Launch::GetScriptStruct() const
{
	return FLayeredMove_Launch::StaticStruct();
}

FString FLayeredMove_Launch::ToSimpleString() const
{
	return FString::Printf(TEXT("Launch"));
}

void FLayeredMove_Launch::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
