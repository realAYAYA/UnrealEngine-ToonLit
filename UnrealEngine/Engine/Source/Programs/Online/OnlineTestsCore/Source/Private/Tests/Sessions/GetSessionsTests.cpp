// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Sessions/CreateSessionHelper.h"
#include "Helpers/Sessions/LeaveSessionHelper.h"
#include "Helpers/Sessions/SendRejectSessionInviteHelper.h"
#include "Helpers/TickForTime.h"
#include "OnlineCatchHelper.h"

#define SESSIONS_TAG "[suite_sessions]"
#define EG_SESSIONS_GETSESSIONS_TAG SESSIONS_TAG "[getsessions]"
#define EG_SESSIONS_GETSESSIONSEOS_TAG SESSIONS_TAG "[getsessions][.EOS]"
#define SESSIONS_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, SESSIONS_TAG __VA_ARGS__)

SESSIONS_TEST_CASE("If I call GetAllSessions with an invalid account id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	GetLoginPipeline()
		.EmplaceLambda([](SubsystemType OnlineSubsystem)
			{
				FGetAllSessions::Params OpGetAllParams;
				OpGetAllParams.LocalAccountId = FAccountId();

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetAllSessions> Result = SessionsInterface->GetAllSessions(MoveTemp(OpGetAllParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidParams());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetAllSessions before creating or joining any sessions, I get a successful result which is an empty array", EG_SESSIONS_GETSESSIONS_TAG)
{
	DestroyCurrentServiceModule();

	FAccountId AccountId;
	GetLoginPipeline(AccountId)
		.EmplaceLambda([&AccountId](SubsystemType OnlineSubsystem)
			{
				FGetAllSessions::Params OpParams;
				OpParams.LocalAccountId = AccountId;

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();				
				TOnlineResult<FGetAllSessions> Result = SessionsInterface->GetAllSessions(MoveTemp(OpParams));
				REQUIRE_OP(Result);
				CHECK(Result.GetOkValue().Sessions.IsEmpty());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetAllSessions with valid conditions, I get a valid array of session references", EG_SESSIONS_GETSESSIONS_TAG)
{
	FAccountId AccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetAllSessionsValidName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 4;
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	CreateSessionHelperParams.OpParams->LocalAccountId = AccountId;

	uint32_t ExpectedSessionsFound = 1;

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceLambda([&AccountId, &ExpectedSessionsFound](SubsystemType OnlineSubsystem)
			{
				FGetAllSessions::Params OpGetAllParams;
				OpGetAllParams.LocalAccountId = AccountId;

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetAllSessions> Result = SessionsInterface->GetAllSessions(MoveTemp(OpGetAllParams));
				REQUIRE_OP(Result);
				CHECK(Result.GetOkValue().Sessions.Num() == ExpectedSessionsFound);
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionByName with an empty session name, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	DestroyCurrentServiceModule();

	FAccountId AccountId;

	GetLoginPipeline(AccountId)
		.EmplaceLambda([](SubsystemType OnlineSubsystem)
			{
				FGetSessionByName::Params OpParams;
				OpParams.LocalName = TEXT("");

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionByName> Result = SessionsInterface->GetSessionByName(MoveTemp(OpParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidParams());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionByName with an unregistered session name, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	FAccountId AccountId;

	GetLoginPipeline(AccountId)
		.EmplaceLambda([](SubsystemType OnlineSubsystem)
			{
				FGetSessionByName::Params OpGetByNameParams;
				OpGetByNameParams.LocalName = TEXT("GetUnregisteredSessionName");

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionByName> Result = SessionsInterface->GetSessionByName(MoveTemp(OpGetByNameParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::NotFound());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionByName with valid information, it returns a valid session reference", EG_SESSIONS_GETSESSIONS_TAG)
{
	ResetAccountStatus();

	FAccountId AccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetSessionByNameValidName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	CreateSessionHelperParams.OpParams->LocalAccountId = AccountId;
	
	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceLambda([](SubsystemType OnlineSubsystem)
			{
				FGetSessionByName::Params OpGetByNameParams;
				OpGetByNameParams.LocalName = TEXT("GetSessionByNameValidName");

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionByName> Result = SessionsInterface->GetSessionByName(MoveTemp(OpGetByNameParams));
				REQUIRE_OP(Result);
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionById with an invalid session id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	DestroyCurrentServiceModule();

	FAccountId AccountId;

	GetLoginPipeline(AccountId)
		.EmplaceLambda([](SubsystemType OnlineSubsystem)
			{
				FGetSessionById::Params OpGetByIdParams;
				OpGetByIdParams.SessionId = FOnlineSessionId();

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionById> Result = SessionsInterface->GetSessionById(MoveTemp(OpGetByIdParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidParams());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionById with a valid but unregistered session id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	ResetAccountStatus();

	FAccountId AccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetUnregisteredSessionByIdName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;

	FLeaveSession::Params OpLeaveParams;
	FLeaveSessionHelper::FHelperParams LeaveSessionHelperParams;
	LeaveSessionHelperParams.OpParams = &OpLeaveParams;
	LeaveSessionHelperParams.OpParams->SessionName = TEXT("GetUnregisteredSessionByIdName");
	LeaveSessionHelperParams.OpParams->bDestroySession = true;

	FGetSessionById::Params OpGetByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	CreateSessionHelperParams.OpParams->LocalAccountId = AccountId;
	LeaveSessionHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceLambda([&OpGetByIdParams](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionByName> Result = SessionsInterface->GetSessionByName({ TEXT("GetUnregisteredSessionByIdName") });
				REQUIRE_OP(Result);

				OpGetByIdParams.SessionId = Result.GetOkValue().Session->GetSessionId();
			})
		.EmplaceStep<FLeaveSessionHelper>(MoveTemp(LeaveSessionHelperParams))
		.EmplaceLambda([&OpGetByIdParams](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionById> Result = SessionsInterface->GetSessionById(MoveTemp(OpGetByIdParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::NotFound());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionById with a valid id for a valid session, I get a valid session reference in return", EG_SESSIONS_GETSESSIONS_TAG)
{
	FAccountId AccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetSessionByValidIdName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;

	FGetSessionById::Params OpGetByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	CreateSessionHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceLambda([&OpGetByIdParams](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionByName> Result = SessionsInterface->GetSessionByName({ TEXT("GetSessionByValidIdName") });
				REQUIRE_OP(Result);

				OpGetByIdParams.SessionId = Result.GetOkValue().Session->GetSessionId();
			})
		.EmplaceLambda([&OpGetByIdParams](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionById> Result = SessionsInterface->GetSessionById(MoveTemp(OpGetByIdParams));
				REQUIRE_OP(Result);
				CHECK(Result.GetOkValue().Session->GetSessionId() == OpGetByIdParams.SessionId);
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetPresenceSession with an invalid id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{

	const int32 NumUsersToLogin = 0;

	GetLoginPipeline(NumUsersToLogin)
		.EmplaceLambda([](SubsystemType OnlineSubsystem)
			{
				FGetPresenceSession::Params OpGetPresenceParams;
				OpGetPresenceParams.LocalAccountId = FAccountId();
	
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetPresenceSession> Result = SessionsInterface->GetPresenceSession(MoveTemp(OpGetPresenceParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidParams());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetPresenceSession with an unregistered id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	FAccountId AccountId;

	GetLoginPipeline(AccountId)
		.EmplaceLambda([&AccountId](SubsystemType OnlineSubsystem)
			{
				FGetPresenceSession::Params OpGetPresenceParams;
				OpGetPresenceParams.LocalAccountId = AccountId;

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetPresenceSession> Result = SessionsInterface->GetPresenceSession(MoveTemp(OpGetPresenceParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidState());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetPresenceSession with a valid id, I get a valid reference to the session", EG_SESSIONS_GETSESSIONS_TAG)
{
	DestroyCurrentServiceModule();
	ResetAccountStatus();


	FAccountId AccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetPresenceSessionWithValidIdName");
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;

	FLeaveSession::Params OpLeaveParams;
	FLeaveSessionHelper::FHelperParams LeaveSessionHelperParams;
	LeaveSessionHelperParams.OpParams = &OpLeaveParams;
	LeaveSessionHelperParams.OpParams->SessionName = TEXT("GetPresenceSessionWithValidIdName");
	LeaveSessionHelperParams.OpParams->bDestroySession = true;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	CreateSessionHelperParams.OpParams->LocalAccountId = AccountId;
	LeaveSessionHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceLambda([&AccountId](SubsystemType OnlineSubsystem)
			{
				FGetPresenceSession::Params OpGetPresenceParams;
				OpGetPresenceParams.LocalAccountId = AccountId;

				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetPresenceSession> Result = SessionsInterface->GetPresenceSession(MoveTemp(OpGetPresenceParams));
				REQUIRE_OP(Result);
			})
		.EmplaceStep<FLeaveSessionHelper>(MoveTemp(LeaveSessionHelperParams));

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionInviteById with an invalid account id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	FGetSessionInviteById::Params OpGetInviteByIdParams;
	OpGetInviteByIdParams.LocalAccountId = FAccountId();

	GetLoginPipeline()
		.EmplaceLambda([&OpGetInviteByIdParams](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionInviteById> Result = SessionsInterface->GetSessionInviteById(MoveTemp(OpGetInviteByIdParams));

				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidParams());

			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionInviteById with an invalid session invite id, I get an error", EG_SESSIONS_GETSESSIONS_TAG)
{
	FAccountId AccountId;

	FGetSessionInviteById::Params OpGetInviteByIdParams;
	OpGetInviteByIdParams.SessionInviteId = FSessionInviteId();
	
	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	OpGetInviteByIdParams.LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceLambda([&OpGetInviteByIdParams](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionInviteById> Result = SessionsInterface->GetSessionInviteById(MoveTemp(OpGetInviteByIdParams));

				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidParams());

			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionInviteById with a valid account id, but without an existing invite, I get an error", EG_SESSIONS_GETSESSIONSEOS_TAG)
{
	DestroyCurrentServiceModule();

	FAccountId FirstAccountId, SecondAccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetInviteByIdWithValidAccountIdName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;

	FSendSessionInvite::Params OpSendInviteParams;
	FSendSessionInviteHelper::FHelperParams SendSessionInviteHelperParams;
	SendSessionInviteHelperParams.OpParams = &OpSendInviteParams;
	SendSessionInviteHelperParams.OpParams->SessionName = TEXT("GetInviteByIdWithValidAccountIdName");

	FGetSessionInviteById::Params OpGetInviteByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);

	OpGetInviteByIdParams.LocalAccountId = FirstAccountId;
	CreateSessionHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendSessionInviteHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendSessionInviteHelperParams.OpParams->TargetUsers.Add(SecondAccountId);

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceStep<FTickForTime>(FTimespan::FromMilliseconds(1000))
		.EmplaceStep<FSendSessionInviteHelper>(MoveTemp(SendSessionInviteHelperParams), [&OpGetInviteByIdParams](const UE::Online::FSessionInviteId& InSessionInviteId)
			{
				OpGetInviteByIdParams.SessionInviteId = InSessionInviteId;
			})
		.EmplaceStep<FTickForTime>(FTimespan::FromMilliseconds(1000))
		.EmplaceLambda([&OpGetInviteByIdParams, &FirstAccountId](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionInviteById> Result = SessionsInterface->GetSessionInviteById(MoveTemp(OpGetInviteByIdParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::InvalidState());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionInviteById with valid invite id, but without invite, I get an error", EG_SESSIONS_GETSESSIONSEOS_TAG)
{
	DestroyCurrentServiceModule();
	ResetAccountStatus();

	FAccountId FirstAccountId, SecondAccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetInviteByIdWithValidInviteIdName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;

	FSendSessionInvite::Params OpSendInviteParams;
	FSendSessionInviteHelper::FHelperParams SendSessionInviteHelperParams;
	SendSessionInviteHelperParams.OpParams = &OpSendInviteParams;
	SendSessionInviteHelperParams.OpParams->SessionName = TEXT("GetInviteByIdWithValidInviteIdName");

	FRejectSessionInvite::Params OpRejectInviteParams;
	FRejectSessionInviteHelper::FHelperParams RejectSessionInviteHelperParams;
	RejectSessionInviteHelperParams.OpParams = &OpRejectInviteParams;

	FGetSessionInviteById::Params OpGetInviteByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);

	OpGetInviteByIdParams.LocalAccountId = SecondAccountId;
	CreateSessionHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendSessionInviteHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendSessionInviteHelperParams.OpParams->TargetUsers.Add(SecondAccountId);
	RejectSessionInviteHelperParams.OpParams->LocalAccountId = SecondAccountId;

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceStep<FTickForTime>(FTimespan::FromMilliseconds(1000))
		.EmplaceStep<FSendSessionInviteHelper>(MoveTemp(SendSessionInviteHelperParams), [&OpGetInviteByIdParams, &RejectSessionInviteHelperParams](const UE::Online::FSessionInviteId& InSessionInviteId)
			{
				OpGetInviteByIdParams.SessionInviteId = InSessionInviteId;
				RejectSessionInviteHelperParams.OpParams->SessionInviteId = InSessionInviteId;
			})
		.EmplaceStep<FTickForTime>(FTimespan::FromMilliseconds(1000))
		.EmplaceStep<FRejectSessionInviteHelper>(MoveTemp(RejectSessionInviteHelperParams))
		.EmplaceLambda([&OpGetInviteByIdParams, &FirstAccountId](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionInviteById> Result = SessionsInterface->GetSessionInviteById(MoveTemp(OpGetInviteByIdParams));
				REQUIRE(Result.IsError());
				CHECK(Result.GetErrorValue() == Errors::NotFound());
			});

	RunToCompletion();
}

SESSIONS_TEST_CASE("If I call GetSessionInviteById with a valid data, I get a valid reference to the session invite", EG_SESSIONS_GETSESSIONSEOS_TAG)
{
	DestroyCurrentServiceModule();
	ResetAccountStatus();

	FAccountId FirstAccountId, SecondAccountId;

	FCreateSession::Params OpCreateParams;
	FCreateSessionHelper::FHelperParams CreateSessionHelperParams;
	CreateSessionHelperParams.OpParams = &OpCreateParams;
	CreateSessionHelperParams.OpParams->SessionName = TEXT("GetInviteByIdValidName");
	CreateSessionHelperParams.OpParams->SessionSettings.SchemaName = TEXT("SchemaName");
	CreateSessionHelperParams.OpParams->SessionSettings.NumMaxConnections = 2;
	CreateSessionHelperParams.OpParams->bPresenceEnabled = true;

	FSendSessionInvite::Params OpSendInviteParams;
	FSendSessionInviteHelper::FHelperParams SendSessionInviteHelperParams;
	SendSessionInviteHelperParams.OpParams = &OpSendInviteParams;
	SendSessionInviteHelperParams.OpParams->SessionName = TEXT("GetInviteByIdValidName");

	FGetSessionInviteById::Params OpGetInviteByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);

	OpGetInviteByIdParams.LocalAccountId = SecondAccountId;
	CreateSessionHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendSessionInviteHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendSessionInviteHelperParams.OpParams->TargetUsers.Add(SecondAccountId);

	LoginPipeline
		.EmplaceStep<FCreateSessionHelper>(MoveTemp(CreateSessionHelperParams))
		.EmplaceStep<FTickForTime>(FTimespan::FromMilliseconds(1000))
		.EmplaceStep<FSendSessionInviteHelper>(MoveTemp(SendSessionInviteHelperParams), [&OpGetInviteByIdParams](const UE::Online::FSessionInviteId& InSessionInviteId)
			{
				OpGetInviteByIdParams.SessionInviteId = InSessionInviteId;
			})
		.EmplaceStep<FTickForTime>(FTimespan::FromMilliseconds(1000))
		.EmplaceLambda([&OpGetInviteByIdParams, &FirstAccountId](SubsystemType OnlineSubsystem)
			{
				ISessionsPtr SessionsInterface = OnlineSubsystem->GetSessionsInterface();
				TOnlineResult<FGetSessionInviteById> Result = SessionsInterface->GetSessionInviteById(MoveTemp(OpGetInviteByIdParams));
				REQUIRE(Result.GetOkValue().SessionInvite->GetInviteId().IsValid());
				CHECK(Result.GetOkValue().SessionInvite->GetSenderId() == FirstAccountId);
			});

	RunToCompletion();
}