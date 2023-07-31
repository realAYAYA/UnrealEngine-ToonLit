// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyLibrary.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimNotifyEndDataContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyLibrary)

bool UAnimNotifyLibrary::NotifyStateReachedEnd(const FAnimNotifyEventReference& EventReference)
{
	const UE::Anim::FAnimNotifyEndDataContext* EndData = EventReference.GetContextData<UE::Anim::FAnimNotifyEndDataContext>();

	if (EndData != nullptr)
	{
		return EndData->bReachedEnd;
	}

	return false;
}

