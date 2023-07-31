// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/SessionsEOSGS.h"

#include "Online/AuthEOSGS.h"
#include "Online/OnlineIdEOSGS.h"
#include "Online/OnlineServicesEOSGS.h"
#include "Online/OnlineServicesEOSGSTypes.h"
#include "Online/NboSerializerEOSGSSvc.h"
#include "Online/OnlineErrorEOSGS.h"
#include "Online/SessionsEOSGSTypes.h"

#include "eos_sessions.h"

namespace UE::Online {

/** FOnlineSessionIdRegistryEOSGS */

FOnlineSessionIdRegistryEOSGS::FOnlineSessionIdRegistryEOSGS()
	: FOnlineSessionIdRegistryLAN(EOnlineServices::Epic)
{
}

FOnlineSessionIdRegistryEOSGS& FOnlineSessionIdRegistryEOSGS::Get()
{
	static FOnlineSessionIdRegistryEOSGS Instance;
	return Instance;
}

/** FOnlineSessionInviteIdRegistryEOSGS */

FOnlineSessionInviteIdRegistryEOSGS::FOnlineSessionInviteIdRegistryEOSGS()
	: FOnlineSessionInviteIdStringRegistry(EOnlineServices::Epic)
{
}

FOnlineSessionInviteIdRegistryEOSGS& FOnlineSessionInviteIdRegistryEOSGS::Get()
{
	static FOnlineSessionInviteIdRegistryEOSGS Instance;
	return Instance;
}

/** FSessionEOSGS */

FSessionEOSGS::FSessionEOSGS(const TSharedPtr<FSessionDetailsHandleEOSGS>& InSessionDetailsHandle)
	: SessionDetailsHandle(InSessionDetailsHandle)
{
	EOS_SessionDetails_CopyInfoOptions CopyInfoOptions = { };
	CopyInfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;
	static_assert(EOS_SESSIONDETAILS_COPYINFO_API_LATEST == 1, "EOS_SessionDetails_CopyInfoOptions updated, check new fields");

	EOS_SessionDetails_Info* SessionDetailsInfo = nullptr;
	EOS_EResult CopyInfoResult = EOS_SessionDetails_CopyInfo(SessionDetailsHandle->SessionDetailsHandle, &CopyInfoOptions, &SessionDetailsInfo);
	if (CopyInfoResult == EOS_EResult::EOS_Success)
	{
		SessionInfo.SessionId = FSessionsEOSGS::CreateSessionId(FString(SessionDetailsInfo->SessionId));
		SessionSettings.CustomSettings.Emplace(EOSGS_HOST_ADDRESS_ATTRIBUTE_KEY, FCustomSessionSetting{ FSchemaVariant(FString(UTF8_TO_TCHAR(SessionDetailsInfo->HostAddress))), ESchemaAttributeVisibility::Public });

		// bInvitesAllowed and bAllowJoinInProgress should both have the same value, and that value corresponds to bAllowNewMembers
		SessionInfo.bAllowSanctionedPlayers = !SessionDetailsInfo->Settings->bSanctionsEnabled;
		SessionSettings.bAllowNewMembers = SessionDetailsInfo->Settings->bInvitesAllowed && SessionDetailsInfo->Settings->bAllowJoinInProgress;
		SessionSettings.NumMaxConnections = SessionDetailsInfo->Settings->NumPublicConnections;
		SessionSettings.JoinPolicy = FromServiceType(SessionDetailsInfo->Settings->PermissionLevel);
		SessionSettings.CustomSettings.Emplace(EOSGS_BUCKET_ID_ATTRIBUTE_KEY, FCustomSessionSetting{ FSchemaVariant(FString(UTF8_TO_TCHAR(SessionDetailsInfo->Settings->BucketId))), ESchemaAttributeVisibility::Public });

		// We retrieve all the session attributes
		EOS_SessionDetails_GetSessionAttributeCountOptions GetAttributeCountOptions = {};
		GetAttributeCountOptions.ApiVersion = EOS_SESSIONDETAILS_GETSESSIONATTRIBUTECOUNT_API_LATEST;
		static_assert(EOS_SESSIONDETAILS_GETSESSIONATTRIBUTECOUNT_API_LATEST == 1, "EOS_SessionDetails_GetSessionAttributeCountOptions updated, check new fields");

		uint32_t AttributeCount = EOS_SessionDetails_GetSessionAttributeCount(SessionDetailsHandle->SessionDetailsHandle, &GetAttributeCountOptions);
		for (uint32_t Index = 0; Index < AttributeCount; ++Index)
		{
			// We parse a single attribute

			EOS_SessionDetails_CopySessionAttributeByIndexOptions CopyAttributeByIndexOptions = {};
			CopyAttributeByIndexOptions.ApiVersion = EOS_SESSIONDETAILS_COPYSESSIONATTRIBUTEBYINDEX_API_LATEST;
			static_assert(EOS_SESSIONDETAILS_COPYSESSIONATTRIBUTEBYINDEX_API_LATEST == 1, "EOS_SessionDetails_CopySessionAttributeByIndexOptions updated, check new fields");

			CopyAttributeByIndexOptions.AttrIndex = Index;

			EOS_SessionDetails_Attribute* Attribute = nullptr;
			EOS_EResult CopyAttributeByIndexResult = EOS_SessionDetails_CopySessionAttributeByIndex(SessionDetailsHandle->SessionDetailsHandle, &CopyAttributeByIndexOptions, &Attribute);
			if (CopyAttributeByIndexResult == EOS_EResult::EOS_Success)
			{
				FString Key(Attribute->Data->Key);
				FSessionAttributeConverter<ESessionAttributeConversionType::FromService> CustomSettingConverter(*Attribute->Data);
				TPair<FSchemaAttributeId, FSchemaVariant> CustomSettingData = CustomSettingConverter.GetAttributeData();
					
				// Most Session Settings values get parsed in the same way as Custom Session Settings, so we will attempt to retrieve them
				if (Key == EOSGS_OWNER_ACCOUNT_ID_ATTRIBUTE_KEY.ToString())
				{
					const FString& PlayerIdStr = CustomSettingData.Value.GetString();
					const EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PlayerIdStr));
					OwnerAccountId = FindAccountId(ProductUserId);
				}
				if (Key == EOSGS_ALLOW_NEW_MEMBERS_ATTRIBUTE_KEY.ToString())
				{
					SessionSettings.bAllowNewMembers = CustomSettingData.Value.GetBoolean();
				}
				else if (Key == EOSGS_ANTI_CHEAT_PROTECTED_ATTRIBUTE_KEY.ToString())
				{
					SessionInfo.bAntiCheatProtected = CustomSettingData.Value.GetBoolean();
				}
				else if (Key == EOSGS_IS_DEDICATED_SERVER_SESSION_ATTRIBUTE_KEY.ToString())
				{
					SessionInfo.bIsDedicatedServerSession = CustomSettingData.Value.GetBoolean();
				}
				else if (Key == EOSGS_SCHEMA_NAME_ATTRIBUTE_KEY.ToString())
				{
					SessionSettings.SchemaName = FSchemaId(CustomSettingData.Value.GetString());
				}
				else // The rest are parsed as a Custom Session Setting
				{
					FCustomSessionSetting CustomSessionSetting;
					CustomSessionSetting.Visibility = FromServiceType(Attribute->AdvertisementType);
					CustomSessionSetting.Data = CustomSettingData.Value;

					SessionSettings.CustomSettings.Emplace(Key, CustomSessionSetting);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[FSessionEOSGS] EOS_SessionDetails_CopySessionAttributeByIndex failed with result [%s]"), *LexToString(CopyInfoResult));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionEOSGS] EOS_SessionDetails_CopyInfo failed with result [%s]"), *LexToString(CopyInfoResult));
	}
}

const FSessionEOSGS& FSessionEOSGS::Cast(const ISession& InSession)
{
	check(InSession.GetSessionId().GetOnlineServicesType() == EOnlineServices::Epic);

	return static_cast<const FSessionEOSGS&>(InSession);
}

/** FSessionsEOSGS */

FSessionsEOSGS::FSessionsEOSGS(FOnlineServicesEOSGS& InServices)
	: Super(InServices)
{
}

void FSessionsEOSGS::Initialize()
{
	Super::Initialize();

	SessionsHandle = EOS_Platform_GetSessionsInterface(static_cast<FOnlineServicesEOSGS&>(GetServices()).GetEOSPlatformHandle());
	check(SessionsHandle);

	RegisterEventHandlers();
}

void FSessionsEOSGS::Shutdown()
{
	Super::Shutdown();

	UnregisterEventHandlers();
}

void FSessionsEOSGS::RegisterEventHandlers()
{
	// Register for session invites received events
	OnSessionInviteReceivedEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		SessionsHandle,
		EOS_SESSIONS_ADDNOTIFYSESSIONINVITERECEIVED_API_LATEST,
		&EOS_Sessions_AddNotifySessionInviteReceived,
		&EOS_Sessions_RemoveNotifySessionInviteReceived,
		&FSessionsEOSGS::HandleSessionInviteReceived);

	static_assert(EOS_SESSIONS_ADDNOTIFYSESSIONINVITERECEIVED_API_LATEST == 1, "EOS_Sessions_AddNotifySessionInviteReceivedOptions updated, check new fields");

	// Register for session invites accepted events
	OnSessionInviteAcceptedEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		SessionsHandle,
		EOS_SESSIONS_ADDNOTIFYSESSIONINVITEACCEPTED_API_LATEST,
		&EOS_Sessions_AddNotifySessionInviteAccepted,
		&EOS_Sessions_RemoveNotifySessionInviteAccepted,
		&FSessionsEOSGS::HandleSessionInviteAccepted);

	static_assert(EOS_SESSIONS_ADDNOTIFYSESSIONINVITEACCEPTED_API_LATEST == 1, "EOS_Sessions_AddNotifySessionInviteAcceptedOptions updated, check new fields");

	// Register for join session accepted events
	OnJoinSessionAcceptedEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		SessionsHandle,
		EOS_SESSIONS_ADDNOTIFYJOINSESSIONACCEPTED_API_LATEST,
		&EOS_Sessions_AddNotifyJoinSessionAccepted,
		&EOS_Sessions_RemoveNotifyJoinSessionAccepted,
		&FSessionsEOSGS::HandleJoinSessionAccepted);

	static_assert(EOS_SESSIONS_ADDNOTIFYJOINSESSIONACCEPTED_API_LATEST == 1, "EOS_Sessions_AddNotifyJoinSessionAcceptedOptions updated, check new fields");
}

void FSessionsEOSGS::UnregisterEventHandlers()
{
	OnSessionInviteReceivedEventRegistration = nullptr;
	OnSessionInviteAcceptedEventRegistration = nullptr;
	OnJoinSessionAcceptedEventRegistration = nullptr;
}

void FSessionsEOSGS::HandleSessionInviteReceived(const EOS_Sessions_SessionInviteReceivedCallbackInfo* Data)
{
	FAccountId LocalAccountId = FindAccountId(Data->LocalUserId);
	if (LocalAccountId.IsValid())
	{
		FString InviteId(UTF8_TO_TCHAR(Data->InviteId));

		BuildSessionFromInvite(LocalAccountId, InviteId)
			.OnComplete([this, WeakThis = AsWeak(), SenderPUID = Data->TargetUserId, InviteId](const TOnlineResult<FBuildSessionFromDetailsHandle>& Result)
		{
			if (Result.IsOk())
			{
				if (const TSharedPtr<ISessions> StrongThis = WeakThis.Pin())
				{
					// First and second place in the array will be occupied by the receiver and the sender, respectively, since the same order is kept in the array of resolved ids
					const FAccountId& ReceiverId = Result.GetOkValue().LocalAccountId;
					const FAccountId& SenderId = FindAccountId(SenderPUID);

					FSessionInviteId SessionInviteId = CreateSessionInviteId(InviteId);

					TSharedRef<FSessionCommon> Session = Result.GetOkValue().Session;

					TSharedRef<FSessionInviteCommon> SessionInviteRef = MakeShared<FSessionInviteCommon>(
						ReceiverId,
						SenderId,
						SessionInviteId,
						Session->GetSessionId()
					);

					AddSessionInvite(SessionInviteRef, Session, ReceiverId);

					FSessionInviteReceived Event = { ReceiverId, SessionInviteId };

					SessionEvents.OnSessionInviteReceived.Broadcast(Event);
				}
			}

			// We won't broadcast the event if there was an error retrieving the session information
		});
	}
}

void FSessionsEOSGS::HandleSessionInviteAccepted(const EOS_Sessions_SessionInviteAcceptedCallbackInfo* Data)
{
	FAccountId LocalAccountId = FindAccountId(Data->LocalUserId);
	if (LocalAccountId.IsValid())
	{
		FString InviteId(UTF8_TO_TCHAR(Data->InviteId));

		BuildSessionFromInvite(LocalAccountId, InviteId)
			.OnComplete([this, WeakThis = AsWeak()](const TOnlineResult<FBuildSessionFromDetailsHandle>& Result)
		{
			if (Result.IsOk())
			{
				if (const TSharedPtr<ISessions> StrongThis = WeakThis.Pin())
				{
					// Instead of using the session information we have stored, we'll use this opportunity to get updated data for the session
					AllSessionsById.Emplace(Result.GetOkValue().Session->GetSessionId(), Result.GetOkValue().Session);

					FUISessionJoinRequested Event{
									Result.GetOkValue().LocalAccountId,
									TResult<FOnlineSessionId, FOnlineError>(Result.GetOkValue().Session->GetSessionId()),
						EUISessionJoinRequestedSource::FromInvitation
					};

					SessionEvents.OnUISessionJoinRequested.Broadcast(Event);

					// The game can react to the OnUISessionJoinRequested event by starting the JoinSession process
				}
			}
		});
	}
}

void FSessionsEOSGS::HandleJoinSessionAccepted(const EOS_Sessions_JoinSessionAcceptedCallbackInfo* Data)
{
	FAccountId LocalAccountId = FindAccountId(Data->LocalUserId);
	if (LocalAccountId.IsValid())
	{
		BuildSessionFromUIEvent(LocalAccountId, Data->UiEventId)
			.OnComplete([this, WeakThis = AsWeak()](const TOnlineResult<FBuildSessionFromDetailsHandle>& Result)
		{
			if (Result.IsOk())
			{
				if (const TSharedPtr<ISessions> StrongThis = WeakThis.Pin())
				{
					// Instead of using the session information we have stored, we'll use this opportunity to get updated data for the session
					AllSessionsById.Emplace(Result.GetOkValue().Session->GetSessionId(), Result.GetOkValue().Session);

					FUISessionJoinRequested Event{
						Result.GetOkValue().LocalAccountId,
						TResult<FOnlineSessionId, FOnlineError>(Result.GetOkValue().Session->GetSessionId()),
			EUISessionJoinRequestedSource::Unspecified
					};

					SessionEvents.OnUISessionJoinRequested.Broadcast(Event);

					// The game can react to the OnUISessionJoinRequested event by starting the JoinSession process
				}
			}
		});
	}
}

TOnlineResult<FSetPresenceSession> FSessionsEOSGS::SetPresenceSession(FSetPresenceSession::Params&& Params)
{
	return TOnlineResult<FSetPresenceSession>(Errors::NotImplemented());
}

TOnlineResult<FClearPresenceSession> FSessionsEOSGS::ClearPresenceSession(FClearPresenceSession::Params&& Params)
{
	return TOnlineResult<FClearPresenceSession>(Errors::NotImplemented());
}

TFuture<TOnlineResult<FCreateSession>> FSessionsEOSGS::CreateSessionImpl(const FCreateSession::Params& Params)
{
	// LAN Sessions
	if (Params.bIsLANSession)
	{
		return FSessionsLAN::CreateSessionImpl(Params);
	}

	// EOSGS Sessions

	TPromise<TOnlineResult<FCreateSession>> Promise;
	TFuture<TOnlineResult<FCreateSession>> Future = Promise.GetFuture();

	if (!Params.SessionIdOverride.IsEmpty())
	{
		int32 Length = Params.SessionIdOverride.Len();

		if (Length < EOS_SESSIONMODIFICATION_MIN_SESSIONIDOVERRIDE_LENGTH || Length > EOS_SESSIONMODIFICATION_MAX_SESSIONIDOVERRIDE_LENGTH)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::CreateSession] Could not create session with SessionIdOverride [%s] of size [%d]. SessionIdOverride size must be between [%d] and [%d] characters long"), *Params.SessionIdOverride, Length, EOS_SESSIONMODIFICATION_MIN_SESSIONIDOVERRIDE_LENGTH, EOS_SESSIONMODIFICATION_MAX_SESSIONIDOVERRIDE_LENGTH);

			Promise.EmplaceValue(Errors::InvalidParams());

			return Future;
		}
	}

	// After all initial checks, we start the session creation operations

	EOS_Sessions_CreateSessionModificationOptions CreateSessionModificationOptions = {};
	CreateSessionModificationOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
	static_assert(EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST == 4, "EOS_Sessions_CreateSessionModificationOptions updated, check new fields");

	CreateSessionModificationOptions.bPresenceEnabled = Params.bPresenceEnabled;
	CreateSessionModificationOptions.bSanctionsEnabled = !Params.bAllowSanctionedPlayers;

	// Check if the Bucket Id custom setting is set, set default otherwise. EOS Sessions can not be created without it
	FString BucketIdStr;
	if (const FCustomSessionSetting* BucketIdSetting = Params.SessionSettings.CustomSettings.Find(EOSGS_BUCKET_ID_ATTRIBUTE_KEY))
	{
		BucketIdStr = BucketIdSetting->Data.GetString();
	}
	else
	{
		int32 BuildUniqueId = GetBuildUniqueId();

		UE_LOG(LogTemp, Verbose, TEXT("[FSessionsEOSGS::CreateSession] 'EOSGS_BUCKET_ID_ATTRIBUTE_KEY' (FString) Custom Setting needed to create EOS sessions not found. Setting \"%d\" as default."), BuildUniqueId);

		BucketIdStr = FString::FromInt(BuildUniqueId);
	}

	const FTCHARToUTF8 BucketIdUtf8(*BucketIdStr);
	CreateSessionModificationOptions.BucketId = BucketIdUtf8.Get();
	
	CreateSessionModificationOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	CreateSessionModificationOptions.MaxPlayers = Params.SessionSettings.NumMaxConnections;

	const FTCHARToUTF8 SessionIdUtf8(*Params.SessionIdOverride);
	if (SessionIdUtf8.Length())
	{
		CreateSessionModificationOptions.SessionId = SessionIdUtf8.Get();
	}

	const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
	CreateSessionModificationOptions.SessionName = SessionNameUtf8.Get();

	EOS_HSessionModification SessionModificationHandle = nullptr;
	EOS_EResult ResultCode = EOS_Sessions_CreateSessionModification(SessionsHandle, &CreateSessionModificationOptions, &SessionModificationHandle);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Error, TEXT("[FSessionsEOSGS::CreateSession] EOS_Sessions_CreateSessionModification failed with result [%s]"), *LexToString(ResultCode));

		Promise.EmplaceValue(Errors::FromEOSResult(ResultCode));

		return Future;
	}

	// We write all SessionSettings values into the SessionModificationHandle
	WriteCreateSessionModificationHandle(SessionModificationHandle, Params);

	// Always update joinability on session creation
	FUpdateSessionImplEOSGS::Params UpdateSessionImplParams{ MakeShared<FSessionModificationHandleEOSGS>(SessionModificationHandle), FUpdateSessionJoinabilityParams{ Params.SessionName, Params.SessionSettings.bAllowNewMembers } };
		
	UpdateSessionImplEOSGS(MoveTemp(UpdateSessionImplParams))
		.Next([this, Promise = MoveTemp(Promise), Params](TDefaultErrorResult<FUpdateSessionImplEOSGS>&& Result) mutable
			{
				if (Result.IsOk())
				{
					// If the session is created successfully, we'll create the local session object
					TSharedRef<FSessionEOSGS> NewSessionEOSGSRef = MakeShared<FSessionEOSGS>();
					NewSessionEOSGSRef->OwnerAccountId = Params.LocalAccountId;
					NewSessionEOSGSRef->SessionSettings = Params.SessionSettings;
					NewSessionEOSGSRef->SessionInfo.SessionId = CreateSessionId(Result.GetOkValue().NewSessionId);
					NewSessionEOSGSRef->SessionInfo.bAllowSanctionedPlayers = Params.bAllowSanctionedPlayers;
					NewSessionEOSGSRef->SessionInfo.bAntiCheatProtected = Params.bAntiCheatProtected;
					NewSessionEOSGSRef->SessionInfo.bIsDedicatedServerSession = IsRunningDedicatedServer();
					NewSessionEOSGSRef->SessionInfo.bIsLANSession = false;
					NewSessionEOSGSRef->SessionInfo.SessionIdOverride = Params.SessionIdOverride;

					AddSessionWithReferences(NewSessionEOSGSRef, Params.SessionName, Params.LocalAccountId, Params.bPresenceEnabled);

					Promise.EmplaceValue(FCreateSession::Result({ }));
				}
				else
				{
					Promise.EmplaceValue(Result.GetErrorValue());
				}
			});

	return Future;
}

void FSessionsEOSGS::SetHostAddress(EOS_HSessionModification& SessionModHandle, FString HostAddress)
{
	// TODO: We could call EOS_SessionModification_SetHostAddress at this point, although it's not necessary
	EOS_SessionModification_SetHostAddressOptions Options = {};
	Options.ApiVersion = EOS_SESSIONMODIFICATION_SETHOSTADDRESS_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_SETHOSTADDRESS_API_LATEST == 1, "EOS_SessionModification_SetHostAddressOptions updated, check new fields");

	const FTCHARToUTF8 HostAddressUtf8(*HostAddress);
	Options.HostAddress = HostAddressUtf8.Get();

	EOS_EResult ResultCode = EOS_SessionModification_SetHostAddress(SessionModHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS_SessionModification_SetHostAddress failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetJoinInProgressAllowed(EOS_HSessionModification& SessionModHandle, bool bIsJoinInProgressAllowed)
{
	EOS_SessionModification_SetJoinInProgressAllowedOptions Options = {};
	Options.ApiVersion = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST == 1, "EOS_SessionModification_SetJoinInProgressAllowedOptions updated, check new fields");

	Options.bAllowJoinInProgress = bIsJoinInProgressAllowed;

	EOS_EResult ResultCode = EOS_SessionModification_SetJoinInProgressAllowed(SessionModHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS_SessionModification_SetJoinInProgressAllowed failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetInvitesAllowed(EOS_HSessionModification& SessionModHandle, bool bAreInvitesAllowed)
{
	EOS_SessionModification_SetInvitesAllowedOptions Options = {};
	Options.ApiVersion = EOS_SESSIONMODIFICATION_SETINVITESALLOWED_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_SETINVITESALLOWED_API_LATEST == 1, "EOS_SessionModification_SetInvitesAllowedOptions updated, check new fields");

	Options.bInvitesAllowed = bAreInvitesAllowed;

	EOS_EResult ResultCode = EOS_SessionModification_SetInvitesAllowed(SessionModHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS_SessionModification_SetInvitesAllowed failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetPermissionLevel(EOS_HSessionModification& SessionModificationHandle, const ESessionJoinPolicy& NewJoinPolicy)
{
	EOS_SessionModification_SetPermissionLevelOptions Options = { };
	Options.ApiVersion = EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST == 1, "EOS_SessionModification_SetPermissionLevelOptions updated, check new fields");

	Options.PermissionLevel = ToServiceType(NewJoinPolicy);

	EOS_EResult ResultCode = EOS_SessionModification_SetPermissionLevel(SessionModificationHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetPermissionLevel] EOS_SessionModification_SetPermissionLevel failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetBucketId(EOS_HSessionModification& SessionModificationHandle, const FString& NewBucketId)
{
	EOS_SessionModification_SetBucketIdOptions Options = { };
	Options.ApiVersion = EOS_SESSIONMODIFICATION_SETBUCKETID_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_SETBUCKETID_API_LATEST == 1, "EOS_SessionModification_SetBucketIdOptions updated, check new fields");

	const FTCHARToUTF8 BucketIdUtf8(*NewBucketId);
	Options.BucketId = BucketIdUtf8.Get();

	EOS_EResult ResultCode = EOS_SessionModification_SetBucketId(SessionModificationHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetBucketId] EOS_SessionModification_SetBucketId failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetMaxPlayers(EOS_HSessionModification& SessionModificationHandle, const uint32& NewMaxPlayers)
{
	EOS_SessionModification_SetMaxPlayersOptions Options = { };
	Options.ApiVersion = EOS_SESSIONMODIFICATION_SETMAXPLAYERS_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_SETMAXPLAYERS_API_LATEST == 1, "EOS_SessionModification_SetMaxPlayersOptions updated, check new fields");

	Options.MaxPlayers = NewMaxPlayers;

	EOS_EResult ResultCode = EOS_SessionModification_SetMaxPlayers(SessionModificationHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetMaxPlayers] EOS_SessionModification_SetMaxPlayers failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::AddAttribute(EOS_HSessionModification& SessionModificationHandle, const FSchemaAttributeId& Key, const FCustomSessionSetting& Value)
{
	EOS_SessionModification_AddAttributeOptions Options = { };
	Options.ApiVersion = EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST == 1, "EOS_SessionModification_AddAttributeOptions updated, check new fields");

	Options.AdvertisementType = ToServiceType(Value.Visibility);

	FSessionAttributeConverter<ESessionAttributeConversionType::ToService> SessionAttribute(Key, Value.Data);
	Options.SessionAttribute = &SessionAttribute.GetAttributeData();

	EOS_EResult ResultCode = EOS_SessionModification_AddAttribute(SessionModificationHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::AddAttribute] EOS_SessionModification_AddAttribute failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::RemoveAttribute(EOS_HSessionModification& SessionModificationHandle, const FSchemaAttributeId& Key)
{
	EOS_SessionModification_RemoveAttributeOptions Options = { };
	Options.ApiVersion = EOS_SESSIONMODIFICATION_REMOVEATTRIBUTE_API_LATEST;
	static_assert(EOS_SESSIONMODIFICATION_REMOVEATTRIBUTE_API_LATEST == 1, "EOS_SessionModification_RemoveAttributeOptions updated, check new fields");

	const FTCHARToUTF8 KeyUtf8(Key.ToString());
	Options.Key = KeyUtf8.Get();

	EOS_EResult ResultCode = EOS_SessionModification_RemoveAttribute(SessionModificationHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::RemoveAttribute] EOS_SessionModification_RemoveAttribute failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::WriteCreateSessionModificationHandle(EOS_HSessionModification& SessionModificationHandle, const FCreateSession::Params& Params)
{
	// We'll enable setting a custom host address if a value for it was set as a custom parameter
	if (const FCustomSessionSetting* HostAddressSetting = Params.SessionSettings.CustomSettings.Find(EOSGS_HOST_ADDRESS_ATTRIBUTE_KEY))
	{
		FString HostAddressStr = HostAddressSetting->Data.GetString();
		if (!HostAddressStr.IsEmpty())
		{
			SetHostAddress(SessionModificationHandle, HostAddressStr);
		}
	}

	// We'll update this setting in the session modification step, and start or end the session accordingly to bock or allow join processes
	SetJoinInProgressAllowed(SessionModificationHandle, Params.SessionSettings.bAllowNewMembers);

	// We'll also update invite permissions for the session
	SetInvitesAllowed(SessionModificationHandle, Params.SessionSettings.bAllowNewMembers);

	// We won't copy bIsLANSession since it' irrelevant for EOS Sessions
	AddAttribute(SessionModificationHandle, EOSGS_ALLOW_NEW_MEMBERS_ATTRIBUTE_KEY, { FSchemaVariant(Params.SessionSettings.bAllowNewMembers), ESchemaAttributeVisibility::Public });
	AddAttribute(SessionModificationHandle, EOSGS_ANTI_CHEAT_PROTECTED_ATTRIBUTE_KEY, { FSchemaVariant(Params.bAntiCheatProtected), ESchemaAttributeVisibility::Public });
	AddAttribute(SessionModificationHandle, EOSGS_IS_DEDICATED_SERVER_SESSION_ATTRIBUTE_KEY, { FSchemaVariant(IsRunningDedicatedServer()), ESchemaAttributeVisibility::Public });

	SetPermissionLevel(SessionModificationHandle, Params.SessionSettings.JoinPolicy);

	SetMaxPlayers(SessionModificationHandle, Params.SessionSettings.NumMaxConnections);

	AddAttribute(SessionModificationHandle, EOSGS_SCHEMA_NAME_ATTRIBUTE_KEY, { FSchemaVariant(Params.SessionSettings.SchemaName.ToString()), ESchemaAttributeVisibility::Public });

	EOS_ProductUserId OwnerPUID = GetProductUserId(Params.LocalAccountId);
	AddAttribute(SessionModificationHandle, EOSGS_OWNER_ACCOUNT_ID_ATTRIBUTE_KEY, { FSchemaVariant(LexToString(OwnerPUID)), ESchemaAttributeVisibility::Public });

	// Custom Settings

 	for (const TPair<FSchemaAttributeId, FCustomSessionSetting>& Entry : Params.SessionSettings.CustomSettings)
 	{
		AddAttribute(SessionModificationHandle, Entry.Key, Entry.Value);
	}

	// BucketId has its own set method on the API
	const FCustomSessionSetting* NewBucketIdSetting = Params.SessionSettings.CustomSettings.Find(EOSGS_BUCKET_ID_ATTRIBUTE_KEY);
	if (NewBucketIdSetting)
	{
		SetBucketId(SessionModificationHandle, NewBucketIdSetting->Data.GetString());
	}	
}

void FSessionsEOSGS::WriteUpdateSessionModificationHandle(EOS_HSessionModification& SessionModificationHandle, const FSessionSettingsUpdate& NewSettings)
{
	// We'll enable setting a custom host address if a value for it was set as a custom parameter
	if (const FCustomSessionSetting* HostAddressSetting = NewSettings.UpdatedCustomSettings.Find(EOSGS_HOST_ADDRESS_ATTRIBUTE_KEY))
	{
		FString HostAddressStr = HostAddressSetting->Data.GetString();
		if (!HostAddressStr.IsEmpty())
		{
			SetHostAddress(SessionModificationHandle, HostAddressStr);
		}
	}

	if (NewSettings.bAllowNewMembers.IsSet())
	{
		// We'll update this setting in the session modification step, and start or end the session accordingly to bock or allow join processes
		SetJoinInProgressAllowed(SessionModificationHandle, NewSettings.bAllowNewMembers.GetValue());

		// We'll also update invite permissions for the session
		SetInvitesAllowed(SessionModificationHandle, NewSettings.bAllowNewMembers.GetValue());
	}

	if (NewSettings.SchemaName.IsSet())
	{
		AddAttribute(SessionModificationHandle, EOSGS_SCHEMA_NAME_ATTRIBUTE_KEY, { FSchemaVariant(NewSettings.SchemaName.GetValue().ToString()), ESchemaAttributeVisibility::Public });
	}

	if (NewSettings.JoinPolicy.IsSet())
	{
		SetPermissionLevel(SessionModificationHandle, NewSettings.JoinPolicy.GetValue());
	}

	if (NewSettings.NumMaxConnections.IsSet())
	{
		SetMaxPlayers(SessionModificationHandle, NewSettings.NumMaxConnections.GetValue());
	}

	// BucketId has its own set method on the API
	const FCustomSessionSetting* NewBucketIdSetting = NewSettings.UpdatedCustomSettings.Find(EOSGS_BUCKET_ID_ATTRIBUTE_KEY);
	if (NewBucketIdSetting)
	{
		SetBucketId(SessionModificationHandle, NewBucketIdSetting->Data.GetString());
	}

	// Custom Settings

	for (const FSchemaAttributeId& Key : NewSettings.RemovedCustomSettings)
	{
		RemoveAttribute(SessionModificationHandle, Key);
	}

	for (const TPair<FSchemaAttributeId, FCustomSessionSetting>& Entry : NewSettings.UpdatedCustomSettings)
	{
		AddAttribute(SessionModificationHandle, Entry.Key, Entry.Value);
	}
}

TFuture<TOnlineResult<FUpdateSessionSettings>> FSessionsEOSGS::UpdateSessionSettingsImpl(const FUpdateSessionSettings::Params& Params)
{
	// LAN Sessions
	TOnlineResult<FGetSessionByName> Result = GetSessionByName({ Params.SessionName });
	if (Result.IsOk() && Result.GetOkValue().Session->GetSessionInfo().bIsLANSession)
	{
		return FSessionsLAN::UpdateSessionSettingsImpl(Params);
	}

	// EOSGS Sessions

	TPromise<TOnlineResult<FUpdateSessionSettings>> Promise;
	TFuture<TOnlineResult<FUpdateSessionSettings>> Future = Promise.GetFuture();

	EOS_Sessions_UpdateSessionModificationOptions UpdateSessionModificationOptions = {};
	UpdateSessionModificationOptions.ApiVersion = EOS_SESSIONS_UPDATESESSIONMODIFICATION_API_LATEST;
	static_assert(EOS_SESSIONS_UPDATESESSIONMODIFICATION_API_LATEST == 1, "EOS_Sessions_UpdateSessionModificationOptions updated, check new fields");

	const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
	UpdateSessionModificationOptions.SessionName = SessionNameUtf8.Get();

	EOS_HSessionModification SessionModificationHandle = nullptr;
	EOS_EResult ResultCode = EOS_Sessions_UpdateSessionModification(SessionsHandle, &UpdateSessionModificationOptions, &SessionModificationHandle);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Error, TEXT("[FSessionsEOSGS::UpdateSession] EOS_Sessions_UpdateSessionModification failed with result [%s]"), *LexToString(ResultCode));

		Promise.EmplaceValue(Errors::FromEOSResult(ResultCode));

		return Future;
	}

	// After creating the session modification handle, we'll add all the updated data to it
	WriteUpdateSessionModificationHandle(SessionModificationHandle, Params.Mutations);

	// Whether we update joinability or not will depend on if bAllowNewMembers was set to a new value
	FUpdateSessionImplEOSGS::Params UpdateSessionImplEOSGSParams =
	{
		MakeShared<FSessionModificationHandleEOSGS>(SessionModificationHandle),
		Params.Mutations.bAllowNewMembers.IsSet() ?
			FUpdateSessionJoinabilityParams{ Params.SessionName, Params.Mutations.bAllowNewMembers.GetValue() } :
			TOptional<FUpdateSessionJoinabilityParams>() // If bAllowNewMembers is not set, the TOptional will be unset too
	};

	UpdateSessionImplEOSGS(MoveTemp(UpdateSessionImplEOSGSParams))
	.Next([this, Promise = MoveTemp(Promise), Params](TDefaultErrorResult<FUpdateSessionImplEOSGS>&& Result) mutable
	{
		if (Result.IsOk())
		{
			TOnlineResult<FGetMutableSessionByName> GetMutableSessionByNameResult = GetMutableSessionByName({ Params.SessionName });
			if (GetMutableSessionByNameResult.IsOk())
			{
				TSharedRef<FSessionCommon> FoundSession = GetMutableSessionByNameResult.GetOkValue().Session;

				// Now that the API Session update has processed successfully, we'll update our local session with the same data
				FSessionUpdate SessionUpdateData = BuildSessionUpdate(FoundSession, Params.Mutations);

				(*FoundSession) += SessionUpdateData;

				// We set the result and fire the event
				Promise.EmplaceValue(FUpdateSessionSettings::Result{ });

				FSessionUpdated SessionUpdatedEvent{ Params.SessionName, SessionUpdateData };
				SessionEvents.OnSessionUpdated.Broadcast(SessionUpdatedEvent);
			}
			else
			{
				Promise.EmplaceValue(GetMutableSessionByNameResult.GetErrorValue());
			}
		}
		else
		{
			Promise.EmplaceValue(Result.GetErrorValue());
		}
	});

	return Future;
}

TFuture<TDefaultErrorResult<FUpdateSessionImplEOSGS>> FSessionsEOSGS::UpdateSessionImplEOSGS(FUpdateSessionImplEOSGS::Params&& Params)
{
	EOS_Sessions_UpdateSessionOptions UpdateSessionOptions = {};
	UpdateSessionOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
	static_assert(EOS_SESSIONS_UPDATESESSION_API_LATEST == 1, "EOS_Sessions_UpdateSessionOptions updated, check new fields");

	UpdateSessionOptions.SessionModificationHandle = Params.SessionModificationHandle->ModificationHandle;

	TPromise<TDefaultErrorResult<FUpdateSessionImplEOSGS>> Promise;
	TFuture<TDefaultErrorResult<FUpdateSessionImplEOSGS>> Future = Promise.GetFuture();

	EOS_Async(EOS_Sessions_UpdateSession, SessionsHandle, UpdateSessionOptions,
	[this, Promise = MoveTemp(Promise), Params = MoveTemp(Params)](const EOS_Sessions_UpdateSessionCallbackInfo* Result) mutable
	{
		// If we only change bAllowNewMembers, the session update will yield an EOS_NoChange result, but we still need to continue to the next step
		if (Result->ResultCode != EOS_EResult::EOS_Success && Result->ResultCode != EOS_EResult::EOS_Sessions_OutOfSync && Result->ResultCode != EOS_EResult::EOS_NoChange)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOS_Sessions_UpdateSession failed with result [%s]"), *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		// After the successful general update, if indicated, we'll update the joinability
		if (Params.UpdateJoinabilitySettings.IsSet())
		{
			UpdateSessionJoinabilityImpl(MoveTemp(Params.UpdateJoinabilitySettings.GetValue()))
				.Next([this, NewSessionId = UTF8_TO_TCHAR(Result->SessionId), Promise = MoveTemp(Promise)](TDefaultErrorResult<FUpdateSessionJoinabilityImpl>&& Result) mutable
			{
				if (Result.IsOk())
				{
					Promise.EmplaceValue(FUpdateSessionImplEOSGS::Result{ NewSessionId });
				}
				else
				{
					Promise.EmplaceValue(Result.GetErrorValue());
				}
			});
		}
		else
		{
			Promise.EmplaceValue(FUpdateSessionImplEOSGS::Result{ FString(Result->SessionId) });
		}
	});

	return Future;
}

TFuture<TDefaultErrorResult<FUpdateSessionJoinabilityImpl>> FSessionsEOSGS::UpdateSessionJoinabilityImpl(FUpdateSessionJoinabilityImpl::Params&& Params)
{
	TPromise<TDefaultErrorResult<FUpdateSessionJoinabilityImpl>> Promise;
	TFuture<TDefaultErrorResult<FUpdateSessionJoinabilityImpl>> Future = Promise.GetFuture();

	// We get the active session handle with the session name
	EOS_Sessions_CopyActiveSessionHandleOptions CopyActiveSessionHandleOptions = {};
	CopyActiveSessionHandleOptions.ApiVersion = EOS_SESSIONS_COPYACTIVESESSIONHANDLE_API_LATEST;
	static_assert(EOS_SESSIONS_COPYACTIVESESSIONHANDLE_API_LATEST == 1, "EOS_Sessions_CopyActiveSessionHandleOptions updated, check new fields");

	const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
	CopyActiveSessionHandleOptions.SessionName = SessionNameUtf8.Get();

	EOS_HActiveSession ActiveSessionHandle;
	EOS_EResult CopyActiveSessionHandleResult = EOS_Sessions_CopyActiveSessionHandle(SessionsHandle, &CopyActiveSessionHandleOptions, &ActiveSessionHandle);
	if (CopyActiveSessionHandleResult != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS_Sessions_CopyActiveSessionHandle failed with result [%s]"), *LexToString(CopyActiveSessionHandleResult));
	}

	// We get the active session info with the handle
	EOS_ActiveSession_CopyInfoOptions CopyInfoOptions = {};
	CopyInfoOptions.ApiVersion = EOS_ACTIVESESSION_COPYINFO_API_LATEST;
	static_assert(EOS_ACTIVESESSION_COPYINFO_API_LATEST == 1, "EOS_ActiveSession_CopyInfoOptions updated, check new fields");

	EOS_ActiveSession_Info* ActiveSessionInfo = nullptr;
	EOS_EResult CopyInfoResult = EOS_ActiveSession_CopyInfo(ActiveSessionHandle, &CopyInfoOptions, &ActiveSessionInfo);
	if (CopyInfoResult != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS_ActiveSession_CopyInfo failed with result [%s]"), *LexToString(CopyInfoResult));
	}

	// If not, we start or end the session to make it joinable or not (as we set JIP to false at creation time)
	if (Params.bAllowNewMembers)
	{
		// We check state. If bAllowNewMembers is true and session has not started, there's no need to do anything
		if (ActiveSessionInfo->State == EOS_EOnlineSessionState::EOS_OSS_InProgress)
		{
			EOS_Sessions_EndSessionOptions Options = {};
			Options.ApiVersion = EOS_SESSIONS_ENDSESSION_API_LATEST;
			static_assert(EOS_SESSIONS_ENDSESSION_API_LATEST == 1, "EOS_Sessions_EndSessionOptions updated, check new fields");

			Options.SessionName = SessionNameUtf8.Get();

			EOS_Async(EOS_Sessions_EndSession, SessionsHandle, Options,
				[this, Promise = MoveTemp(Promise)](const EOS_Sessions_EndSessionCallbackInfo* Result) mutable
			{
				if (Result->ResultCode != EOS_EResult::EOS_Success && Result->ResultCode != EOS_EResult::EOS_Sessions_OutOfSync)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOS_Sessions_EndSession failed with result [%s]"), *LexToString(Result->ResultCode));
					Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
					return;
				}

				Promise.EmplaceValue(FUpdateSessionJoinabilityImpl::Result{ });
			});
		}
		else
		{
			Promise.EmplaceValue(FUpdateSessionJoinabilityImpl::Result{ });
		}
	}
	else
	{
		// We check state. If bAllowNewMembers is false and the session has started, there's no need to do anything
		if (ActiveSessionInfo->State == EOS_EOnlineSessionState::EOS_OSS_Pending || ActiveSessionInfo->State == EOS_EOnlineSessionState::EOS_OSS_Ended)
		{
			EOS_Sessions_StartSessionOptions Options = {};
			Options.ApiVersion = EOS_SESSIONS_STARTSESSION_API_LATEST;
			static_assert(EOS_SESSIONS_STARTSESSION_API_LATEST == 1, "EOS_Sessions_StartSessionOptions updated, check new fields");

			Options.SessionName = SessionNameUtf8.Get();

			EOS_Async(EOS_Sessions_StartSession, SessionsHandle, Options,
				[this, Promise = MoveTemp(Promise)](const EOS_Sessions_StartSessionCallbackInfo* Result) mutable
			{
				if (Result->ResultCode != EOS_EResult::EOS_Success && Result->ResultCode != EOS_EResult::EOS_Sessions_OutOfSync)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOS_Sessions_StartSession failed with result [%s]"), *LexToString(Result->ResultCode));
					Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
					return;
				}

				Promise.EmplaceValue(FUpdateSessionJoinabilityImpl::Result{ });
			});
		}
		else
		{
			Promise.EmplaceValue(FUpdateSessionJoinabilityImpl::Result{ });
		}
	}

	return Future;
}

TOnlineAsyncOpHandle<FSendSingleSessionInviteImpl> FSessionsEOSGS::SendSingleSessionInviteImpl(FSendSingleSessionInviteImpl::Params&& Params)
{
	TOnlineAsyncOpRef<FSendSingleSessionInviteImpl> Op = GetOp<FSendSingleSessionInviteImpl>(MoveTemp(Params));
	const FSendSingleSessionInviteImpl::Params& OpParams = Op->GetParams();

	EOS_Sessions_SendInviteOptions SendInviteOptions = { };
	SendInviteOptions.ApiVersion = EOS_SESSIONS_SENDINVITE_API_LATEST;
	static_assert(EOS_SESSIONS_SENDINVITE_API_LATEST == 1, "EOS_Sessions_SendInviteOptions updated, check new fields");

	SendInviteOptions.LocalUserId = GetProductUserIdChecked(OpParams.LocalAccountId);

	const FTCHARToUTF8 SessionNameUtf8(*OpParams.SessionName.ToString());
	SendInviteOptions.SessionName = SessionNameUtf8.Get();

	SendInviteOptions.TargetUserId = GetProductUserIdChecked(OpParams.TargetAccountId);

	EOS_Async(EOS_Sessions_SendInvite, SessionsHandle, SendInviteOptions,
		[this, WeakOp = Op->AsWeak()](const EOS_Sessions_SendInviteCallbackInfo* Result) mutable
	{
		if (TOnlineAsyncOpPtr<FSendSingleSessionInviteImpl> StrongOp = WeakOp.Pin())
		{
			if (Result->ResultCode != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("EOS_Sessions_SendInvite failed with result [%s]"), *LexToString(Result->ResultCode));
				StrongOp->SetError(Errors::FromEOSResult(Result->ResultCode));
				return;
			}

			StrongOp->SetResult(FSendSingleSessionInviteImpl::Result{ });
		}
	});

	return Op->GetHandle();
}

TFuture<TOnlineResult<FLeaveSession>> FSessionsEOSGS::LeaveSessionImpl(const FLeaveSession::Params& Params)
{
	// LAN Sessions
	TOnlineResult<FGetSessionByName> Result = GetSessionByName({ Params.SessionName });
	if (Result.IsOk() && Result.GetOkValue().Session->GetSessionInfo().bIsLANSession)
	{
		return FSessionsLAN::LeaveSessionImpl(Params);
	}

	// EOSGS Sessions

	TPromise<TOnlineResult<FLeaveSession>> Promise;
	TFuture<TOnlineResult<FLeaveSession>> Future = Promise.GetFuture();

	EOS_Sessions_DestroySessionOptions DestroySessionOptions = {};
	DestroySessionOptions.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;
	static_assert(EOS_SESSIONS_DESTROYSESSION_API_LATEST == 1, "EOS_Sessions_DestroySessionOptions updated, check new fields");

	const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
	DestroySessionOptions.SessionName = SessionNameUtf8.Get();

	EOS_Async(EOS_Sessions_DestroySession, SessionsHandle, DestroySessionOptions,
		[this, Promise = MoveTemp(Promise), Params](const EOS_Sessions_DestroySessionCallbackInfo* Result) mutable
		{
			if (Result->ResultCode != EOS_EResult::EOS_Success)
			{
				Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
				return;
			}

			TOnlineResult<FGetSessionByName> GetSessionByNameResult = GetSessionByName({ Params.SessionName });
			if (GetSessionByNameResult.IsOk())
			{
				TSharedRef<const ISession> FoundSession = GetSessionByNameResult.GetOkValue().Session;

				ClearSessionReferences(FoundSession->GetSessionId(), Params.SessionName, Params.LocalAccountId);

				Promise.EmplaceValue(FLeaveSession::Result{ });

				FSessionLeft SessionLeftEvent;
				SessionLeftEvent.LocalAccountId = Params.LocalAccountId;
				SessionEvents.OnSessionLeft.Broadcast(SessionLeftEvent);
			}
			else
			{
				Promise.EmplaceValue(GetSessionByNameResult.GetErrorValue());
			}
		});

	return Future;
}

void FSessionsEOSGS::SetSessionSearchMaxResults(FSessionSearchHandleEOSGS& SessionSearchHandle, uint32 MaxResults)
{
	EOS_SessionSearch_SetMaxResultsOptions Options = { };
	Options.ApiVersion = EOS_SESSIONSEARCH_SETMAXSEARCHRESULTS_API_LATEST;
	static_assert(EOS_SESSIONSEARCH_SETMAXSEARCHRESULTS_API_LATEST == 1, "EOS_SessionSearch_SetMaxResultsOptions updated, check new fields");

	// We truncate the max results parameter up to the value of EOS_SESSIONS_MAX_SEARCH_RESULTS
	Options.MaxSearchResults = FMath::Clamp(MaxResults, 1, EOS_SESSIONS_MAX_SEARCH_RESULTS);

	EOS_EResult ResultCode = EOS_SessionSearch_SetMaxResults(SessionSearchHandle.SearchHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetSessionSearchMaxResults] EOS_SessionSearch_SetMaxResults failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetSessionSearchParameters(FSessionSearchHandleEOSGS& SessionSearchHandle, TArray<FFindSessionsSearchFilter> Filters)
{
	for (const FFindSessionsSearchFilter& Filter : Filters)
	{
		EOS_SessionSearch_SetParameterOptions Options = { };
		Options.ApiVersion = EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST;
		static_assert(EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST == 1, "EOS_SessionSearch_SetMaxResultsOptions updated, check new fields");

		Options.ComparisonOp = ToServiceType(Filter.ComparisonOp);
		
		FSessionAttributeConverter<ESessionAttributeConversionType::ToService> Parameter(Filter.Key, Filter.Value);
		Options.Parameter = &Parameter.GetAttributeData();

		EOS_EResult ResultCode = EOS_SessionSearch_SetParameter(SessionSearchHandle.SearchHandle, &Options);
		if (ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetSessionSearchParameters] EOS_SessionSearch_SetParameter failed with result [%s]"), *LexToString(ResultCode));
		}
	}
}

void FSessionsEOSGS::SetSessionSearchSessionId(FSessionSearchHandleEOSGS& SessionSearchHandle, const FOnlineSessionId& SessionId)
{
	EOS_SessionSearch_SetSessionIdOptions Options = { };
	Options.ApiVersion = EOS_SESSIONSEARCH_SETSESSIONID_API_LATEST;
	static_assert(EOS_SESSIONSEARCH_SETSESSIONID_API_LATEST == 1, "EOS_SessionSearch_SetSessionIdOptions updated, check new fields");

	const FTCHARToUTF8 SessionIdUtf8(*FOnlineSessionIdRegistryEOSGS::Get().ToLogString(SessionId));
	Options.SessionId = SessionIdUtf8.Get();

	EOS_EResult ResultCode = EOS_SessionSearch_SetSessionId(SessionSearchHandle.SearchHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetSessionSearchSessionId] EOS_SessionSearch_SetSessionId failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::SetSessionSearchTargetId(FSessionSearchHandleEOSGS& SessionSearchHandle, const FAccountId& TargetAccountId)
{
	EOS_SessionSearch_SetTargetUserIdOptions Options = { };
	Options.ApiVersion = EOS_SESSIONSEARCH_SETTARGETUSERID_API_LATEST;
	static_assert(EOS_SESSIONSEARCH_SETTARGETUSERID_API_LATEST == 1, "EOS_SessionSearch_SetTargetUserIdOptions updated, check new fields");
	Options.TargetUserId = GetProductUserIdChecked(TargetAccountId);

	EOS_EResult ResultCode = EOS_SessionSearch_SetTargetUserId(SessionSearchHandle.SearchHandle, &Options);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::SetSessionSearchTargetId] EOS_SessionSearch_SetTargetUserId failed with result [%s]"), *LexToString(ResultCode));
	}
}

void FSessionsEOSGS::WriteSessionSearchHandle(FSessionSearchHandleEOSGS& SessionSearchHandle, const FFindSessions::Params& Params)
{
	SetSessionSearchMaxResults(SessionSearchHandle, Params.MaxResults);

	if (!Params.Filters.IsEmpty())
	{
		SetSessionSearchParameters(SessionSearchHandle, Params.Filters);
	}

	if (Params.SessionId.IsSet())
	{
		SetSessionSearchSessionId(SessionSearchHandle, Params.SessionId.GetValue());
	}

	if (Params.TargetUser.IsSet())
	{
		SetSessionSearchTargetId(SessionSearchHandle, Params.TargetUser.GetValue());
	}
}

FOnlineSessionId FSessionsEOSGS::CreateSessionId(const FString& SessionId)
{
	return FOnlineSessionIdRegistryEOSGS::Get().BasicRegistry.FindOrAddHandle(SessionId);
}

FSessionInviteId FSessionsEOSGS::CreateSessionInviteId(const FString& SessionInviteId) const
{
	return FOnlineSessionInviteIdRegistryEOSGS::Get().BasicRegistry.FindOrAddHandle(SessionInviteId);
}

TFuture<TOnlineResult<FFindSessions>> FSessionsEOSGS::FindSessionsImpl(const FFindSessions::Params& Params)
{
	// LAN Sessions
	if (Params.bFindLANSessions)
	{
		return FSessionsLAN::FindSessionsImpl(Params);
	}

	// EOSGS Sessions
	TPromise<TOnlineResult<FFindSessions>> Promise;
	TFuture<TOnlineResult<FFindSessions>> Future = Promise.GetFuture();

	// We start preparing the search
	EOS_Sessions_CreateSessionSearchOptions CreateSessionSearchOptions = { };
	CreateSessionSearchOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST;
	static_assert(EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST == 1, "EOS_Sessions_CreateSessionSearchOptions updated, check new fields");
	CreateSessionSearchOptions.MaxSearchResults = FMath::Clamp(Params.MaxResults, 0, EOS_SESSIONS_MAX_SEARCH_RESULTS);

	EOS_HSessionSearch SearchHandle = nullptr;
	EOS_EResult ResultCode = EOS_Sessions_CreateSessionSearch(SessionsHandle, &CreateSessionSearchOptions, &SearchHandle);
	if (ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Error, TEXT("[FSessionsEOSGS::FindSessions] EOS_Sessions_CreateSessionSearch failed with result [%s]"), *LexToString(ResultCode));

		Promise.EmplaceValue(Errors::FromEOSResult(ResultCode));
		return Future;
	}

	const TSharedRef<FSessionSearchHandleEOSGS>& CurrentSessionSearchHandleEOSGS = CurrentSessionSearchHandleEOSGSUserMap.Emplace(Params.LocalAccountId, MakeShared<FSessionSearchHandleEOSGS>(SearchHandle));

	// Before we start the search, we reset the cache and save the promise
	SearchResultsUserMap.FindOrAdd(Params.LocalAccountId).Reset();
	CurrentSessionSearchPromisesUserMap.Emplace(Params.LocalAccountId, MoveTemp(Promise));

	// We write the search attributes
	WriteSessionSearchHandle(*CurrentSessionSearchHandleEOSGS, Params);

	EOS_SessionSearch_FindOptions FindOptions = { };
	FindOptions.ApiVersion = EOS_SESSIONSEARCH_FIND_API_LATEST;
	static_assert(EOS_SESSIONSEARCH_FIND_API_LATEST == 2, "EOS_SessionSearch_FindOptions updated, check new fields");
	FindOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);

	EOS_Async(EOS_SessionSearch_Find, CurrentSessionSearchHandleEOSGS->SearchHandle, FindOptions,
		[this, Params](const EOS_SessionSearch_FindCallbackInfo* FindCallbackInfoResult) mutable
		{
			TPromise<TOnlineResult<FFindSessions>>& Promise = CurrentSessionSearchPromisesUserMap.FindChecked(Params.LocalAccountId);

			if (FindCallbackInfoResult->ResultCode != EOS_EResult::EOS_Success)
			{
				Promise.EmplaceValue(Errors::FromEOSResult(FindCallbackInfoResult->ResultCode));
				return;
			}

			const TSharedRef<FSessionSearchHandleEOSGS>& CurrentSessionSearchHandleEOSGS = CurrentSessionSearchHandleEOSGSUserMap.FindChecked(Params.LocalAccountId);

			// For a successful session, we'll get the search results
			EOS_SessionSearch_GetSearchResultCountOptions GetSearchResultCountOptions = { };
			GetSearchResultCountOptions.ApiVersion = EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
			static_assert(EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST == 1, "EOS_SessionSearch_GetSearchResultCountOptions updated, check new fields");
			int32 NumSearchResults = EOS_SessionSearch_GetSearchResultCount(CurrentSessionSearchHandleEOSGS->SearchHandle, &GetSearchResultCountOptions);

			EOS_SessionSearch_CopySearchResultByIndexOptions CopySearchResultByIndexOptions = { };
			CopySearchResultByIndexOptions.ApiVersion = EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
			static_assert(EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST == 1, "EOS_SessionSearch_CopySearchResultByIndexOptions updated, check new fields");

			TArray<TFuture<TDefaultErrorResult<FBuildSessionFromDetailsHandle>>> PendingSessionsBuilt;

			for (int32 Index = 0; Index < NumSearchResults; Index++)
			{
				EOS_HSessionDetails SessionDetailsHandle = nullptr;

				CopySearchResultByIndexOptions.SessionIndex = Index;
				EOS_EResult CopySearchResultByIndexResult = EOS_SessionSearch_CopySearchResultByIndex(CurrentSessionSearchHandleEOSGS->SearchHandle, &CopySearchResultByIndexOptions, &SessionDetailsHandle);
				if (CopySearchResultByIndexResult == EOS_EResult::EOS_Success)
				{
					TSharedRef<TPromise<TDefaultErrorResult<FBuildSessionFromDetailsHandle>>> BuildSessionPromise = MakeShared<TPromise<TDefaultErrorResult<FBuildSessionFromDetailsHandle>>>();
					BuildSessionFromDetailsHandle({ Params.LocalAccountId, MakeShared<FSessionDetailsHandleEOSGS>(SessionDetailsHandle) })
						.OnComplete([this, BuildSessionPromise](const TOnlineResult<FBuildSessionFromDetailsHandle>& Result) mutable
					{
						if (Result.IsOk())
						{
							AddSearchResult(Result.GetOkValue().Session, Result.GetOkValue().LocalAccountId);

							BuildSessionPromise->EmplaceValue(Result.GetOkValue());
						}
						else
						{
							BuildSessionPromise->EmplaceValue(Result.GetErrorValue());
						}
					});
			
					PendingSessionsBuilt.Add(BuildSessionPromise->GetFuture());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[FSessionsEOSGS::FindSessions] EOS_SessionSearch_CopySearchResultByIndex failed for index [%d] with result [%s]"), Index, *LexToString(CopySearchResultByIndexResult));

					Promise.EmplaceValue(Errors::FromEOSResult(CopySearchResultByIndexResult));
					return;
				}
			}

			WhenAll(MoveTemp(PendingSessionsBuilt))
				.Next([this, Params](TArray<TDefaultErrorResult<FBuildSessionFromDetailsHandle>>&& Results) mutable
			{
				TPromise<TOnlineResult<FFindSessions>>& Promise = CurrentSessionSearchPromisesUserMap.FindChecked(Params.LocalAccountId);

				for (TDefaultErrorResult<FBuildSessionFromDetailsHandle>& Result : Results)
				{
					if (Result.IsError())
					{
						// Store first encountered error to return as result.
						Promise.EmplaceValue(MoveTemp(Result.GetErrorValue()));
						return;
					}
				}

				TArray<FOnlineSessionId>& SearchResults = SearchResultsUserMap.FindChecked(Params.LocalAccountId);

				Promise.EmplaceValue(FFindSessions::Result{ SearchResults });
				CurrentSessionSearchPromisesUserMap.Remove(Params.LocalAccountId);
				CurrentSessionSearchHandleEOSGSUserMap.Remove(Params.LocalAccountId);
			});
		});

	return Future;
}

TFuture<TOnlineResult<FJoinSession>> FSessionsEOSGS::JoinSessionImpl(const FJoinSession::Params& Params)
{
	TPromise<TOnlineResult<FJoinSession>> Promise;
	TFuture<TOnlineResult<FJoinSession>> Future = Promise.GetFuture();

	TOnlineResult<FGetSessionById> GetSessionByIdResult = GetSessionById({ Params.SessionId });
	if (GetSessionByIdResult.IsError())
	{
		// If no result is found, the id might be expired, which we should notify
		if (FOnlineSessionIdRegistryEOSGS::Get().IsSessionIdExpired(Params.SessionId))
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] SessionId parameter [%s] is expired. Please call FindSessions to get an updated list of available sessions "), UTF8_TO_TCHAR(__FUNCTION__), *ToLogString(Params.SessionId));
		}

		Promise.EmplaceValue(GetSessionByIdResult.GetErrorValue());
		return Future;
	}

	const TSharedRef<const ISession>& FoundSession = GetSessionByIdResult.GetOkValue().Session;

	// LAN Sessions
	if (FoundSession->GetSessionInfo().bIsLANSession)
	{
		return FSessionsLAN::JoinSessionImpl(Params);
	}

	// EOSGS Sessions

	// We check that the passed session has a valid details handle
	const FSessionEOSGS& SessionEOSGS = FSessionEOSGS::Cast(*FoundSession);
	if (!SessionEOSGS.SessionDetailsHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Could not join session with invalid session details handle in session with id [%s]"), UTF8_TO_TCHAR(__FUNCTION__), *ToLogString(Params.SessionId));

		Promise.EmplaceValue(Errors::InvalidState());
		return Future;
	}

	// We start setup for the API call
	EOS_Sessions_JoinSessionOptions JoinSessionOptions = { };
	JoinSessionOptions.ApiVersion = EOS_SESSIONS_JOINSESSION_API_LATEST;
	static_assert(EOS_SESSIONS_JOINSESSION_API_LATEST == 2, "EOS_Sessions_JoinSessionOptions updated, check new fields");

	JoinSessionOptions.bPresenceEnabled = Params.bPresenceEnabled;

	JoinSessionOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);

	const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
	JoinSessionOptions.SessionName = SessionNameUtf8.Get();

	JoinSessionOptions.SessionHandle = SessionEOSGS.SessionDetailsHandle->SessionDetailsHandle;

	EOS_Async(EOS_Sessions_JoinSession, SessionsHandle, JoinSessionOptions, 
		[this, Promise = MoveTemp(Promise), Params](const EOS_Sessions_JoinSessionCallbackInfo* Result) mutable
		{
			if (Result->ResultCode != EOS_EResult::EOS_Success)
			{
				Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
				return;
			}
		
			TOnlineResult<FGetSessionById> GetSessionByIdResult = GetSessionById({ Params.SessionId });
			if (GetSessionByIdResult.IsError())
			{
				// If no result is found, the id might be expired, which we should notify
				if (FOnlineSessionIdRegistryEOSGS::Get().IsSessionIdExpired(Params.SessionId))
				{
					UE_LOG(LogTemp, Warning, TEXT("[%s] SessionId parameter [%s] is expired. Please call FindSessions to get an updated list of available sessions "), UTF8_TO_TCHAR(__FUNCTION__), *ToLogString(Params.SessionId));
				}

				Promise.EmplaceValue(MoveTemp(GetSessionByIdResult.GetErrorValue()));
				return;
			}

			const TSharedRef<const ISession>& FoundSession = GetSessionByIdResult.GetOkValue().Session;

			AddSessionReferences(FoundSession->GetSessionId(), Params.SessionName, Params.LocalAccountId, Params.bPresenceEnabled);

			// After successfully joining a session, we'll remove all related invites if any are found
			ClearSessionInvitesForSession(Params.LocalAccountId, FoundSession->GetSessionId());

			Promise.EmplaceValue(FJoinSession::Result{ });

			FSessionJoined Event = { { Params.LocalAccountId }, FoundSession->GetSessionId() };

			SessionEvents.OnSessionJoined.Broadcast(Event);

			// A successful join allows the client to server travel, after which AddSessionMember will be called by the engine
		});

	return Future;
}

TOnlineAsyncOpHandle<FBuildSessionFromDetailsHandle> FSessionsEOSGS::BuildSessionFromInvite(const FAccountId& LocalAccountId, const FString& InInviteId)
{
	EOS_Sessions_CopySessionHandleByInviteIdOptions CopySessionHandleByInviteIdOptions = { };
	CopySessionHandleByInviteIdOptions.ApiVersion = EOS_SESSIONS_COPYSESSIONHANDLEBYINVITEID_API_LATEST;
	static_assert(EOS_SESSIONS_COPYSESSIONHANDLEBYINVITEID_API_LATEST == 1, "EOS_Sessions_CopySessionHandleByInviteIdOptions updated, check new fields");

	const FTCHARToUTF8 InviteIdUtf8(*InInviteId);
	CopySessionHandleByInviteIdOptions.InviteId = InviteIdUtf8.Get();

	EOS_HSessionDetails SessionDetailsHandle;
	EOS_EResult CopySessionHandleByInviteIdResult = EOS_Sessions_CopySessionHandleByInviteId(SessionsHandle, &CopySessionHandleByInviteIdOptions, &SessionDetailsHandle);
	if (CopySessionHandleByInviteIdResult == EOS_EResult::EOS_Success)
	{
		return BuildSessionFromDetailsHandle({ LocalAccountId, MakeShared<FSessionDetailsHandleEOSGS>(SessionDetailsHandle) });
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FSessionsEOSGS::BuildSessionFromInvite] EOS_Sessions_CopySessionHandleByInviteId failed with result [%s]"), *LexToString(CopySessionHandleByInviteIdResult));

		TOnlineAsyncOpPtr<FBuildSessionFromDetailsHandle> Operation;
		Operation->SetError(Errors::FromEOSResult(CopySessionHandleByInviteIdResult));
		return Operation->GetHandle();
	}
}

TOnlineAsyncOpHandle<FBuildSessionFromDetailsHandle> FSessionsEOSGS::BuildSessionFromUIEvent(const FAccountId& LocalAccountId, const EOS_UI_EventId& UIEventId)
{
	EOS_Sessions_CopySessionHandleByUiEventIdOptions CopySessionHandleByUiEventIdOptions = { };
	CopySessionHandleByUiEventIdOptions.ApiVersion = EOS_SESSIONS_COPYSESSIONHANDLEBYUIEVENTID_API_LATEST;
	static_assert(EOS_SESSIONS_COPYSESSIONHANDLEBYUIEVENTID_API_LATEST == 1, "EOS_Sessions_CopySessionHandleByUiEventIdOptions updated, check new fields");

	CopySessionHandleByUiEventIdOptions.UiEventId = UIEventId;

	EOS_HSessionDetails SessionDetailsHandle;
	EOS_EResult CopySessionHandleByUiEventIdResult = EOS_Sessions_CopySessionHandleByUiEventId(SessionsHandle, &CopySessionHandleByUiEventIdOptions, &SessionDetailsHandle);
	if (CopySessionHandleByUiEventIdResult == EOS_EResult::EOS_Success)
	{
		return BuildSessionFromDetailsHandle({ LocalAccountId, MakeShared<FSessionDetailsHandleEOSGS>(SessionDetailsHandle) });
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FSessionsEOSGS::BuildSessionFromUIEvent] EOS_Sessions_CopySessionHandleByUiEventId failed with result [%s]"), *LexToString(CopySessionHandleByUiEventIdResult));

		TOnlineAsyncOpPtr<FBuildSessionFromDetailsHandle> Operation;
		Operation->SetError(Errors::FromEOSResult(CopySessionHandleByUiEventIdResult));
		return Operation->GetHandle();
	}
}

TResult<TArray<EOS_ProductUserId>, FOnlineError> GetProductUserIdsFromEOSGSSession(const EOS_HSessionDetails& SessionDetailsHandle)
{
	TArray<EOS_ProductUserId> Result;

	EOS_SessionDetails_CopyInfoOptions CopyInfoOptions = { };
	CopyInfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;
	static_assert(EOS_SESSIONDETAILS_COPYINFO_API_LATEST == 1, "EOS_SessionDetails_CopyInfoOptions updated, check new fields");

	EOS_SessionDetails_Info* SessionDetailsInfo = nullptr;
	EOS_EResult CopyInfoResult = EOS_SessionDetails_CopyInfo(SessionDetailsHandle, &CopyInfoOptions, &SessionDetailsInfo);
	if (CopyInfoResult == EOS_EResult::EOS_Success)
	{
		// We retrieve all the session attributes
		EOS_SessionDetails_GetSessionAttributeCountOptions GetAttributeCountOptions = {};
		GetAttributeCountOptions.ApiVersion = EOS_SESSIONDETAILS_GETSESSIONATTRIBUTECOUNT_API_LATEST;
		static_assert(EOS_SESSIONDETAILS_GETSESSIONATTRIBUTECOUNT_API_LATEST == 1, "EOS_SessionDetails_GetSessionAttributeCountOptions updated, check new fields");
		
		uint32_t AttributeCount = EOS_SessionDetails_GetSessionAttributeCount(SessionDetailsHandle, &GetAttributeCountOptions);
		for (uint32_t Index = 0; Index < AttributeCount; ++Index)
		{
			EOS_SessionDetails_CopySessionAttributeByIndexOptions CopyAttributeByIndexOptions = {};
			CopyAttributeByIndexOptions.ApiVersion = EOS_SESSIONDETAILS_COPYSESSIONATTRIBUTEBYINDEX_API_LATEST;
			static_assert(EOS_SESSIONDETAILS_COPYSESSIONATTRIBUTEBYINDEX_API_LATEST == 1, "EOS_SessionDetails_CopySessionAttributeByIndexOptions updated, check new fields");

			CopyAttributeByIndexOptions.AttrIndex = Index;

			EOS_SessionDetails_Attribute* Attribute = nullptr;
			EOS_EResult CopyAttributeByIndexResult = EOS_SessionDetails_CopySessionAttributeByIndex(SessionDetailsHandle, &CopyAttributeByIndexOptions, &Attribute);
			if (CopyAttributeByIndexResult == EOS_EResult::EOS_Success)
			{
				// We parse a single attribute
				FString Key(Attribute->Data->Key);

				if (Key == EOSGS_OWNER_ACCOUNT_ID_ATTRIBUTE_KEY.ToString())
				{
					FSessionAttributeConverter<ESessionAttributeConversionType::FromService> CustomSettingConverter(*Attribute->Data);
					TPair<FSchemaAttributeId, FSchemaVariant> CustomSettingData = CustomSettingConverter.GetAttributeData();

					const FString& PlayerIdStr = CustomSettingData.Value.GetString();
					const EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PlayerIdStr));
					Result.AddUnique(ProductUserId);

					break;
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::BuildSessionFromDetailsHandle] EOS_SessionDetails_CopySessionAttributeByIndex failed with result [%s]"), *LexToString(CopyAttributeByIndexResult));

				TResult<TArray<EOS_ProductUserId>, FOnlineError>(Errors::FromEOSResult(CopyAttributeByIndexResult));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FSessionsEOSGS::BuildSessionFromDetailsHandle] EOS_SessionDetails_CopyInfo failed with result [%s]"), *LexToString(CopyInfoResult));

		TResult<TArray<EOS_ProductUserId>, FOnlineError>(Errors::FromEOSResult(CopyInfoResult));
	}

	return TResult<TArray<EOS_ProductUserId>, FOnlineError>(Result);
}

TOnlineAsyncOpHandle<FBuildSessionFromDetailsHandle> FSessionsEOSGS::BuildSessionFromDetailsHandle(FBuildSessionFromDetailsHandle::Params&& Params)
{
	TOnlineAsyncOpRef<FBuildSessionFromDetailsHandle> Op = GetOp<FBuildSessionFromDetailsHandle>(MoveTemp(Params));
	const FBuildSessionFromDetailsHandle::Params& OpParams = Op->GetParams();

	// The first step in the process will be retrieving all ids in the session that need resolution
	TResult<TArray<EOS_ProductUserId>, FOnlineError> GetIdsResult = GetProductUserIdsFromEOSGSSession(Params.SessionDetailsHandleEOSGS->SessionDetailsHandle);

	if (GetIdsResult.IsOk())
	{
		Services.Get<FAuthEOSGS>()->ResolveAccountIds(Params.LocalAccountId, GetIdsResult.GetOkValue())
			.Next([this, WeakOp = Op->AsWeak(), Params = MoveTemp(Params)](TArray<FAccountId> ResolvedAccountIds) mutable
		{
			if (TOnlineAsyncOpPtr<FBuildSessionFromDetailsHandle> StrongOp = WeakOp.Pin())
			{
				// After all the ids are resolved, we can build the session safely
				StrongOp->SetResult(FBuildSessionFromDetailsHandle::Result{ StrongOp->GetParams().LocalAccountId, MakeShared<FSessionEOSGS>(Params.SessionDetailsHandleEOSGS) });
			}
		});
	}
	else
	{
		Op->SetError(MoveTemp(GetIdsResult.GetErrorValue()));
	}

	return Op->GetHandle();
}

TFuture<TOnlineResult<FSendSessionInvite>> FSessionsEOSGS::SendSessionInviteImpl(const FSendSessionInvite::Params& Params)
{
	TPromise<TOnlineResult<FSendSessionInvite>> Promise;
	TFuture<TOnlineResult<FSendSessionInvite>> Future = Promise.GetFuture();

	TArray<TFuture<TDefaultErrorResult<FSendSingleSessionInviteImpl>>> PendingSessionInvites;
	for (const FAccountId& TargetAccountId : Params.TargetUsers)
	{
		FSendSingleSessionInviteImpl::Params FSendSingleSessionInviteParams;
		FSendSingleSessionInviteParams.LocalAccountId = Params.LocalAccountId;
		FSendSingleSessionInviteParams.SessionName = Params.SessionName;
		FSendSingleSessionInviteParams.TargetAccountId = TargetAccountId;

		TSharedRef<TPromise<TDefaultErrorResult<FSendSingleSessionInviteImpl>>> SessionInvitePromise = MakeShared<TPromise<TDefaultErrorResult<FSendSingleSessionInviteImpl>>>();
		SendSingleSessionInviteImpl(MoveTemp(FSendSingleSessionInviteParams))
			.OnComplete([SessionInvitePromise](const TOnlineResult<FSendSingleSessionInviteImpl>& Result)
				{
					if (Result.IsOk())
					{
						SessionInvitePromise->EmplaceValue(Result.GetOkValue());
					}
					else
					{
						SessionInvitePromise->EmplaceValue(Result.GetErrorValue());
					}
				});

		PendingSessionInvites.Emplace(SessionInvitePromise->GetFuture());
	}

	WhenAll(MoveTemp(PendingSessionInvites))
		.Next([Promise = MoveTemp(Promise)](TArray<TDefaultErrorResult<FSendSingleSessionInviteImpl>>&& Results) mutable
	{
		for (TDefaultErrorResult<FSendSingleSessionInviteImpl>& Result : Results)
		{
			if (Result.IsError())
			{
				Promise.EmplaceValue(MoveTemp(Result.GetErrorValue()));
				return;
			}
		}

		Promise.EmplaceValue(FSendSessionInvite::Result{ });
	});

	return Future;
}

TFuture<TOnlineResult<FRejectSessionInvite>> FSessionsEOSGS::RejectSessionInviteImpl(const FRejectSessionInvite::Params& Params)
{
	TPromise<TOnlineResult<FRejectSessionInvite>> Promise;
	TFuture<TOnlineResult<FRejectSessionInvite>> Future = Promise.GetFuture();

	EOS_Sessions_RejectInviteOptions RejectInviteOptions = { };
	RejectInviteOptions.ApiVersion = EOS_SESSIONS_REJECTINVITE_API_LATEST;
	static_assert(EOS_SESSIONS_REJECTINVITE_API_LATEST == 1, "EOS_Sessions_RejectInviteOptions updated, check new fields");

	RejectInviteOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);

	const FString& InviteIdStr = FOnlineSessionInviteIdRegistryEOSGS::Get().BasicRegistry.FindIdValue(Params.SessionInviteId);
	const FTCHARToUTF8 InviteIdUtf8(*InviteIdStr);
	RejectInviteOptions.InviteId = InviteIdUtf8.Get();

	EOS_Async(EOS_Sessions_RejectInvite, SessionsHandle, RejectInviteOptions, 
		[this, Promise = MoveTemp(Promise), Params](const EOS_Sessions_RejectInviteCallbackInfo* Result) mutable
		{
			if (Result->ResultCode != EOS_EResult::EOS_Success)
			{
				Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
				return;
			}

			if (TMap<FSessionInviteId, TSharedRef<FSessionInviteCommon>>* UserMap = SessionInvitesUserMap.Find(Params.LocalAccountId))
			{
				UserMap->Remove(Params.SessionInviteId);
			}

			Promise.EmplaceValue(FRejectSessionInvite::Result{ });
		});

	return Future;
}

TFuture<TOnlineResult<FAddSessionMember>> FSessionsEOSGS::AddSessionMemberImpl(const FAddSessionMember::Params& Params)
{
	// LAN Sessions
	TOnlineResult<FGetSessionByName> Result = GetSessionByName({ Params.SessionName });
	if (Result.IsOk() && Result.GetOkValue().Session->GetSessionInfo().bIsLANSession)
	{
		return FSessionsLAN::AddSessionMemberImpl(Params);
	}

	// EOSGS Sessions

	TPromise<TOnlineResult<FAddSessionMember>> Promise;
	TFuture<TOnlineResult<FAddSessionMember>> Future = Promise.GetFuture();

	Services.Get<FAuthEOSGS>()->ResolveAccountIds(Params.LocalAccountId, { GetProductUserIdChecked(Params.LocalAccountId) })
		.Next([this, Promise = MoveTemp(Promise), Params](TArray<FAccountId> ResolvedAccountIds) mutable
			{
				if (!ResolvedAccountIds.IsEmpty())
				{
					EOS_Sessions_RegisterPlayersOptions Options = { };
					Options.ApiVersion = EOS_SESSIONS_REGISTERPLAYERS_API_LATEST;
					static_assert(EOS_SESSIONS_REGISTERPLAYERS_API_LATEST == 2, "EOS_Sessions_RegisterPlayersOptions updated, check new fields");

					Options.PlayersToRegisterCount = 1;

					TArray<EOS_ProductUserId> PlayersToRegister = { GetProductUserIdChecked(Params.LocalAccountId) };
					Options.PlayersToRegister = PlayersToRegister.GetData();

					const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
					Options.SessionName = SessionNameUtf8.Get();

					EOS_Async(EOS_Sessions_RegisterPlayers, SessionsHandle, Options,
						[this, Promise = MoveTemp(Promise), Params](const EOS_Sessions_RegisterPlayersCallbackInfo* Result) mutable
						{
							if (Result->ResultCode != EOS_EResult::EOS_Success)
							{
								Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
								return;
							}

							Promise.EmplaceValue(AddSessionMemberInternal(Params));
						});
				}
				else
				{
					Promise.EmplaceValue(Errors::InvalidUser());
				}
			});

	return Future;
}

TFuture<TOnlineResult<FRemoveSessionMember>> FSessionsEOSGS::RemoveSessionMemberImpl(const FRemoveSessionMember::Params& Params)
{
	// LAN Sessions
	TOnlineResult<FGetSessionByName> Result = GetSessionByName({ Params.SessionName });
	if (Result.IsOk() && Result.GetOkValue().Session->GetSessionInfo().bIsLANSession)
	{
		return FSessionsLAN::RemoveSessionMemberImpl(Params);
	}

	// EOSGS Sessions

	TPromise<TOnlineResult<FRemoveSessionMember>> Promise;
	TFuture<TOnlineResult<FRemoveSessionMember>> Future = Promise.GetFuture();

	Services.Get<FAuthEOSGS>()->ResolveAccountIds(Params.LocalAccountId, { GetProductUserIdChecked(Params.LocalAccountId) })
		.Next([this, Promise = MoveTemp(Promise), Params](TArray<FAccountId> ResolvedAccountIds) mutable
			{
				if (!ResolvedAccountIds.IsEmpty())
				{
					EOS_Sessions_UnregisterPlayersOptions Options = { };
					Options.ApiVersion = EOS_SESSIONS_UNREGISTERPLAYERS_API_LATEST;
					static_assert(EOS_SESSIONS_UNREGISTERPLAYERS_API_LATEST == 2, "EOS_Sessions_UnregisterPlayersOptions updated, check new fields");

					Options.PlayersToUnregisterCount = 1;

					TArray<EOS_ProductUserId> PlayersToUnregister{ GetProductUserIdChecked(Params.LocalAccountId) };
					Options.PlayersToUnregister = PlayersToUnregister.GetData();

					const FTCHARToUTF8 SessionNameUtf8(*Params.SessionName.ToString());
					Options.SessionName = SessionNameUtf8.Get();

					EOS_Async(EOS_Sessions_UnregisterPlayers, SessionsHandle, Options,
						[this, Promise = MoveTemp(Promise), Params](const EOS_Sessions_UnregisterPlayersCallbackInfo* Result) mutable
						{
							if (Result->ResultCode != EOS_EResult::EOS_Success)
							{
								Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
								return;
							}

							Promise.EmplaceValue(RemoveSessionMemberInternal(Params));
						});
				}
				else
				{
					Promise.EmplaceValue(Errors::InvalidUser());
				}
			});

	return Future;
}

/** FSessionsLAN */

void FSessionsEOSGS::AppendSessionToPacket(FNboSerializeToBuffer& Packet, const FSessionLAN& Session)
{
	using namespace NboSerializerLANSvc;
	using namespace NboSerializerEOSGSSvc;

	SerializeToBuffer(Packet, Session);
	SerializeToBuffer(Packet, Session.OwnerAccountId);
	SerializeToBuffer(Packet, Session.SessionInfo.SessionId);
	SerializeToBuffer(Packet, Session.SessionMembers);
}

void FSessionsEOSGS::ReadSessionFromPacket(FNboSerializeFromBuffer& Packet, FSessionLAN& Session)
{
	using namespace NboSerializerLANSvc;
	using namespace NboSerializerEOSGSSvc;

	SerializeFromBuffer(Packet, Session);
	SerializeFromBuffer(Packet, Session.OwnerAccountId);
	SerializeFromBuffer(Packet, Session.SessionInfo.SessionId);
	SerializeFromBuffer(Packet, Session.SessionMembers);
}

/* UE::Online */ }