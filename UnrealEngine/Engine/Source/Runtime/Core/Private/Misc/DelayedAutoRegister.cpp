// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/DelayedAutoRegister.h"

#include "Containers/Set.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateBase.h"
#include "Delegates/DelegateInstancesImpl.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Function.h"

DECLARE_MULTICAST_DELEGATE(FDelayedAutoRegisterDelegate);

static TSet<EDelayedRegisterRunPhase> GPhasesAlreadyRun;

static FDelayedAutoRegisterDelegate& GetDelayedAutoRegisterDelegate(EDelayedRegisterRunPhase Phase)
{
	static FDelayedAutoRegisterDelegate Singleton[(uint8)EDelayedRegisterRunPhase::NumPhases];
	return Singleton[(uint8)Phase];
}

FDelayedAutoRegisterHelper::FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase Phase, TFunction<void()> RegistrationFunction)
{
	// if the phase has already passed, we just run the function immediately
	if (GPhasesAlreadyRun.Contains(Phase))
	{
		RegistrationFunction();
	}
	else
	{
		GetDelayedAutoRegisterDelegate(Phase).AddLambda(RegistrationFunction);
	}
}

void FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase Phase)
{
	checkf(!GPhasesAlreadyRun.Contains(Phase), TEXT("Delayed Startup phase %d has already run - it is not expected to be run again!"), (int32)Phase);

	// run all the delayed functions!
	GetDelayedAutoRegisterDelegate(Phase).Broadcast();
	GetDelayedAutoRegisterDelegate(Phase).Clear();

	GPhasesAlreadyRun.Add(Phase);
}
