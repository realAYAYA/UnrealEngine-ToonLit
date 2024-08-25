// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"

#include "RigUnit_AnimNextGraphSelf.generated.h"

class UAnimNextGraph;

/** Get a reference to the currently executing graph */
USTRUCT(meta=(DisplayName="Self", Category="Graph", NodeColor="0, 0, 1", Keywords="Current,This"))
struct ANIMNEXT_API FRigUnit_AnimNextGraphSelf : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	// The currently-executing graph
	UPROPERTY(meta=(Output))
	TObjectPtr<UAnimNextGraph> Self;
};
