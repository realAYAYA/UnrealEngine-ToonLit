// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosSolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosSolver)

DEFINE_LOG_CATEGORY_STATIC(FSC_Log, NoLogging, All);

UChaosSolver::UChaosSolver(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
	check(ObjectInitializer.GetClass() == GetClass());
}



