// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/CachedAnimDataLibrary.h"
#include "Animation/CachedAnimData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CachedAnimDataLibrary)

UCachedAnimDataLibrary::UCachedAnimDataLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCachedAnimDataLibrary::StateMachine_IsStateRelevant(UAnimInstance* InAnimInstance, const FCachedAnimStateData& CachedAnimStateData)
{
	if (InAnimInstance)
	{
		return CachedAnimStateData.IsRelevant(*InAnimInstance);
	}

	return false;
}

float UCachedAnimDataLibrary::StateMachine_GetLocalWeight(UAnimInstance* InAnimInstance, const FCachedAnimStateData& CachedAnimStateData)
{
	if (InAnimInstance)
	{
		return CachedAnimStateData.GetWeight(*InAnimInstance);
	}

	return 0.0f;
}

float UCachedAnimDataLibrary::StateMachine_GetGlobalWeight(UAnimInstance* InAnimInstance, const FCachedAnimStateData& CachedAnimStateData)
{
	if (InAnimInstance)
	{
		return CachedAnimStateData.GetGlobalWeight(*InAnimInstance);
	}

	return 0.0f;
}

