// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/TestReplicatedActor.h"

#include "Net/UnrealNetwork.h"

// -----------------------------------------------------------------------------

ATestReplicatedActor::ATestReplicatedActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ReplicatedInt(0)
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

// -----------------------------------------------------------------------------

void ATestReplicatedActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATestReplicatedActor, ReplicatedInt);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
