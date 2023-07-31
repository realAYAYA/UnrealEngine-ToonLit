// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassRepresentationSubsystem.h"

#include "MassCrowdRepresentationSubsystem.generated.h"

/**
 * Subsystem responsible for all visual of mass crowd agents, will handle actors spawning and static mesh instances
 */
UCLASS()
class MASSCROWD_API UMassCrowdRepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem END
};