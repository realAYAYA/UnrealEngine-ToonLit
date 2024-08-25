// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAvaTransitionModule.h"
#include "AvaTransitionLog.h"

DEFINE_LOG_CATEGORY(LogAvaTransition);

class FAvaTransitionModule : public IAvaTransitionModule
{
	virtual FOnValidateTransitionTree& GetOnValidateTransitionTree()
	{
		return OnValidateStateTree;
	}

	FOnValidateTransitionTree OnValidateStateTree;
};

IMPLEMENT_MODULE(FAvaTransitionModule, AvalancheTransition)
