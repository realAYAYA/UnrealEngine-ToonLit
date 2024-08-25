// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TestDriver.h"
#include "TestHarness.h"

#include "Online/AuthCommon.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineServicesCommon.h"
#include "OnlineCatchHelper.h"

using namespace UE::Online;

struct FAuthLoginStep : public FTestPipeline::FStep
{
	FAuthLoginStep(UE::Online::FAuthLogin::Params&& InLocalAccount)
		: LocalAccount(MoveTemp(InLocalAccount))
	{
	}

	virtual ~FAuthLoginStep()
	{
		if (OnlineAuthPtr != nullptr)
		{
			OnlineAuthPtr = nullptr;
		}
	};

	enum class EState { Init, LoginCalled, Done } State = EState::Init;


	virtual EContinuance Tick(SubsystemType OnlineSubsystem) override
	{
		OnlineAuthPtr = OnlineSubsystem->GetAuthInterface();

		switch (State)
		{
		case EState::Init:
		{
			State = EState::LoginCalled;

			FPlatformUserId PlatformUserId = LocalAccount.PlatformUserId;

			TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> LocalOnlineUserResult = OnlineAuthPtr->GetLocalOnlineUserByPlatformUserId({PlatformUserId});
			if (LocalOnlineUserResult.IsOk() && LocalOnlineUserResult.GetOkValue().AccountInfo->LoginStatus == UE::Online::ELoginStatus::LoggedIn)
			{
				State = EState::Done;
			}
			else
			{
				OnlineAuthPtr->Login(MoveTemp(LocalAccount))
				 .OnComplete([this, PlatformUserId](const TOnlineResult<FAuthLogin> Op) mutable
				{
					CHECK(State == EState::LoginCalled);
					CHECK_OP_EQ(Op, Errors::NotImplemented());

					if(Op.IsOk())
					{
						CHECK_OP(OnlineAuthPtr->GetLocalOnlineUserByOnlineAccountId({Op.GetOkValue().AccountInfo->AccountId}));
					}
					else if (Op.GetErrorValue() == Errors::NotImplemented())
					{
						// Some auth implementations do not have an explicit login / logout. In those implementations all platform users are assumed to always be logged in.
						TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> OnlineUserResult = OnlineAuthPtr->GetLocalOnlineUserByPlatformUserId({PlatformUserId});
						CHECK_OP(OnlineUserResult);
					}

					State = EState::Done;
				});
			}

			break;
		}
		case EState::LoginCalled:
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
	int32 LocalUserNum = 0;
	UE::Online::FAuthLogin::Params LocalAccount;
	UE::Online::IAuthPtr OnlineAuthPtr = nullptr;
	FDelegateHandle	OnLoginCompleteDelegateHandle;
};