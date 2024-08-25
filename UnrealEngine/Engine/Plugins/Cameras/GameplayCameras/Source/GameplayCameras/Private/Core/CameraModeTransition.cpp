// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraModeTransition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraModeTransition)

bool UCameraModeTransitionCondition::TransitionMatches(const FCameraModeTransitionConditionMatchParams& Params) const
{
	return OnTransitionMatches(Params);
}

