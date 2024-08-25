// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TestDriver.h"
#include "TestHarness.h"

#include "Online/AuthCommon.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineServicesCommon.h"

struct FAuthLogoutStep : public FTestPipeline::FStep
{
	FAuthLogoutStep(FPlatformUserId InPlatformUserId)
		: PlatformUserId(InPlatformUserId)
	{}

	virtual ~FAuthLogoutStep()
	{
		OnlineAuthPtr = nullptr;
	};


	enum class EState { Init, LogoutCalled, Done } State = EState::Init;


	virtual EContinuance Tick(SubsystemType OnlineSubsystem) override
	{
		OnlineAuthPtr = OnlineSubsystem->GetAuthInterface();

		switch (State)
		{
		case EState::Init:
		{
			State = EState::LogoutCalled;
			TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> AccountId = OnlineAuthPtr->GetLocalOnlineUserByPlatformUserId({PlatformUserId});
			CAPTURE(ToLogString(AccountId), PlatformUserId);
			CHECK_OP(AccountId);
			if(AccountId.IsOk())
			{
				OnlineAuthPtr->Logout({ AccountId.GetOkValue().AccountInfo->AccountId})
					.OnComplete([this](const TOnlineResult<FAuthLogout> Op) mutable
					{
						CHECK(State == EState::LogoutCalled);
						// Some implementations do not implement an explicit login / logout.
						CHECK_OP_EQ(Op, Errors::NotImplemented());

						State = EState::Done;
					});
			}
			else
			{
				State = EState::Done;
			}

			break;
		}
		case EState::LogoutCalled:
		{
			break;
		}
		case EState::Done:
		{
			return EContinuance::Done;
		}
		}

		return EContinuance::ContinueStepping;
	}
protected:
	FPlatformUserId PlatformUserId;
	UE::Online::IAuthPtr OnlineAuthPtr = nullptr;
	FDelegateHandle	OnLogoutCompleteDelegateHandle;
};