// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AsyncTestStep.h"
#include "Online/Presence.h"
#include "OnlineCatchHelper.h"

using namespace UE::Online;
class FPartialUpdatePresenceHelper : public FAsyncTestStep
{
public:
	FPartialUpdatePresenceHelper(FPartialUpdatePresence::Params&& InParams)
		: Params(MoveTemp(InParams))
	{

	}

	virtual void Run(FAsyncStepResult Promise, SubsystemType Services) override
	{
		IPresencePtr Presence = Services->GetPresenceInterface();

		Presence->PartialUpdatePresence(MoveTemp(Params))
		.OnComplete([this, Promise](const TOnlineResult<FPartialUpdatePresence> Result) mutable
		{
			CHECK_OP(Result);
			Promise->SetValue(true);
		});
	}

protected:
	FPartialUpdatePresence::Params Params;
};