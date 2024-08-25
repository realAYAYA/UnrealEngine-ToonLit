// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h"
#include "RigUnit_ConnectionCandidates.generated.h"

/**
 * Returns the connection candidates for one connector
 */
USTRUCT(meta=(DisplayName="Get Candidates", Category="Modules", TitleColor="1 0 0", NodeColor="1 1 1", Keywords="Connection,Resolve", Varying))
struct CONTROLRIG_API FRigUnit_GetCandidates : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	// The connector being resolved
	UPROPERTY(EditAnywhere, Transient, Category = "Modules", meta = (Output))
	FRigElementKey Connector;

	// The items being interacted on
	UPROPERTY(EditAnywhere, Transient, Category = "Modules", meta = (Output))
	TArray<FRigElementKey> Candidates;
};

/**
 * Discards matches during a connector event
 */
USTRUCT(meta=(DisplayName="Discard Matches", Category="Modules", TitleColor="1 0 0", NodeColor="1 1 1", Keywords="Connection,Resolve,Match", Varying))
struct CONTROLRIG_API FRigUnit_DiscardMatches : public FRigUnitMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	// The items being interacted on
	UPROPERTY(EditAnywhere, Transient, Category = "Modules", meta = (Input))
	TArray<FRigElementKey> Excluded;

	UPROPERTY(EditAnywhere, Transient, Category = "Modules", meta = (Input))
	FString Message;
};

/**
 * Set default match during a connector event
 */
USTRUCT(meta=(DisplayName="Set Default Match", Category="Modules", TitleColor="1 0 0", NodeColor="1 1 1", Keywords="Connection,Resolve,Match,Default", Varying))
struct CONTROLRIG_API FRigUnit_SetDefaultMatch : public FRigUnitMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	// The items being interacted on
	UPROPERTY(EditAnywhere, Transient, Category = "Modules", meta = (Input))
	FRigElementKey Default;
};

