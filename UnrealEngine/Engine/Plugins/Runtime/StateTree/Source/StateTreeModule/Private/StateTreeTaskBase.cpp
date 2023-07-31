// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTaskBase.h"
#include "CoreMinimal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTaskBase)


#if WITH_GAMEPLAY_DEBUGGER
void FStateTreeTaskBase::AppendDebugInfoString(FString& DebugString, const FStateTreeExecutionContext& Context) const
{
	DebugString += FString::Printf(TEXT("[%s]\n"), *Name.ToString());
}
#endif

