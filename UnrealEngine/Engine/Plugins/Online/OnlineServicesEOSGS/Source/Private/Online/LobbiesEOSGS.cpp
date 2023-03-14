// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/LobbiesEOSGS.h"

#include "Online/AuthEOSGS.h"
#include "Online/LobbiesEOSGSTypes.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineIdEOSGS.h"
#include "Online/OnlineServicesEOSGS.h"
#include "Online/OnlineServicesEOSGSTypes.h"

#include "IEOSSDKManager.h"

#include "eos_lobby.h"

namespace UE::Online {

static const FString LobbyDataKeyName = TEXT("LobbyData");
static const FString LobbyDetailsKeyName = TEXT("LobbyDetails");
static const FString LobbyIdStringKeyName = TEXT("LobbyIdString");
static const FString LobbySearchKeyName = TEXT("LobbySearch");

static const int32 MaxAttributeSize = 1000;

FLobbiesEOSGS::FLobbiesEOSGS(FOnlineServicesEOSGS& InServices)
	: FLobbiesCommon(InServices)
{
}

void FLobbiesEOSGS::Initialize()
{
	FLobbiesCommon::Initialize();

	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	check(SDKManager);

	EOS_HLobby LobbyInterfaceHandle = EOS_Platform_GetLobbyInterface(static_cast<FOnlineServicesEOSGS&>(GetServices()).GetEOSPlatformHandle());
	check(LobbyInterfaceHandle != nullptr);

	LobbyPrerequisites = MakeShared<FLobbyPrerequisitesEOS>(FLobbyPrerequisitesEOS{
		LobbyInterfaceHandle,
		StaticCastSharedPtr<FAuthEOSGS>(Services.GetAuthInterface()),
		{SDKManager->GetProductName(), GetBuildUniqueId()},
		SchemaRegistry});

	LobbyDataRegistry = MakeShared<FLobbyDataRegistryEOS>(LobbyPrerequisites.ToSharedRef());

	RegisterHandlers();
}

void FLobbiesEOSGS::PreShutdown()
{
	UnregisterHandlers();

	LobbyDataRegistry = nullptr;
	LobbyPrerequisites = nullptr;
}

TOnlineAsyncOpHandle<FCreateLobby> FLobbiesEOSGS::CreateLobby(FCreateLobby::Params&& InParams)
{
	// Helper lambda for simplifying error handling.
	auto DestroyLobbyDuringCreate = [this](
		TOnlineAsyncOp<FCreateLobby>& InAsyncOp,
		FAccountId LocalAccountId,
		const FString& LobbyIdString,
		FOnlineError ErrorResult) -> TFuture<void>
	{
		FLobbiesDestroyLobbyImpl::Params DestroyLobbyParams;
		DestroyLobbyParams.LobbyIdString = LobbyIdString;
		DestroyLobbyParams.LocalAccountId = LocalAccountId;

		TPromise<void> Promise;
		auto Future = Promise.GetFuture();

		DestroyLobbyImpl(MoveTemp(DestroyLobbyParams))
		.Next(
		[
			InAsyncOp = InAsyncOp.AsShared(),
			LocalAccountId,
			LobbyIdString,
			ErrorResult = MoveTemp(ErrorResult),
			Promise = MoveTemp(Promise)
		](TDefaultErrorResult<FLobbiesDestroyLobbyImpl>&& Result) mutable
		{
			if (Result.IsError())
			{
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] DestroyLobbyImpl Failed: User[%s], Lobby[%s], Result[%s]"),
					*ToLogString(LocalAccountId), *LobbyIdString, *Result.GetErrorValue().GetLogString());
			}

			// Set operation error to the passed in error value.
			InAsyncOp->SetError(MoveTemp(ErrorResult));
			Promise.EmplaceValue();
		});

		return Future;
	};

	TOnlineAsyncOpRef<FCreateLobby> Op = GetOp<FCreateLobby>(MoveTemp(InParams));
	const FCreateLobby::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	// Check that requested lobby schema exists.
	if (!SchemaRegistry->GetDefinition(Params.SchemaId).IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] Failed: Unknown lobby schema [%s]"), *Params.SchemaId.ToString().ToLower());
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	// Start operation.
	// Step 1: Call create lobby.
	Op->Then([this](TOnlineAsyncOp<FCreateLobby>& InAsyncOp, TPromise<const EOS_Lobby_CreateLobbyCallbackInfo*>&& Promise)
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();
		const FLobbyBucketIdTranslator<ELobbyTranslationType::ToService> BucketTanslator(LobbyPrerequisites->BucketId);

		// The lobby will be created as invitation only.
		// Once all local members are joined and the lobby attributes have been set the privacy setting will be moved to the user setting.

		EOS_Lobby_CreateLobbyOptions CreateLobbyOptions = {};
		CreateLobbyOptions.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
		CreateLobbyOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
		CreateLobbyOptions.MaxLobbyMembers = Params.MaxMembers;
		// Prevent lobby from appearing in search results until fully created with all user attributes.
		CreateLobbyOptions.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY;
		CreateLobbyOptions.bPresenceEnabled = false; // todo: handle
		CreateLobbyOptions.bAllowInvites = true; // todo: handle
		CreateLobbyOptions.BucketId = BucketTanslator.GetBucketIdEOS();
		CreateLobbyOptions.bDisableHostMigration = false; // todo: handle
		CreateLobbyOptions.bEnableRTCRoom = false; // todo: handle
		static_assert(EOS_LOBBY_CREATELOBBY_API_LATEST == 8, "EOS_Lobby_CreateLobbyOptions updated, check new fields");

		EOS_Async(EOS_Lobby_CreateLobby, LobbyPrerequisites->LobbyInterfaceHandle, CreateLobbyOptions, MoveTemp(Promise)); 
	})
	// Step 2: Handle errors and attach the lobby id to the operation data.
	.Then([this](TOnlineAsyncOp<FCreateLobby>& InAsyncOp, const EOS_Lobby_CreateLobbyCallbackInfo* Data)
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();

		if (Data->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] EOS_Lobby_CreateLobby Failed: User[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LexToString(Data->ResultCode));
			InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
		}
		else
		{
			// Store the lobby id string on the operation data. This is used to destroy the lobby when creating the lobby structures fails.
			InAsyncOp.Data.Set<FString>(LobbyIdStringKeyName, FString(UTF8_TO_TCHAR(Data->LobbyId)));
		}
	})
	// Step 3: Create the lobby details object from the lobby id.
	.Then([this, DestroyLobbyDuringCreate](TOnlineAsyncOp<FCreateLobby>& InAsyncOp)
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();
		const FString& LobbyIdString = GetOpDataChecked<FString>(InAsyncOp, LobbyIdStringKeyName);

		// Try to create the lobby details object.
		TDefaultErrorResultInternal<TSharedRef<FLobbyDetailsEOS>> Result =
			FLobbyDetailsEOS::CreateFromLobbyId(LobbyPrerequisites.ToSharedRef(), Params.LocalAccountId, LobbyIdString);
		if (Result.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] FLobbyDetailsEOS::CreateFromLobbyId Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyIdString, *Result.GetErrorValue().GetLogString());

			return DestroyLobbyDuringCreate(InAsyncOp, Params.LocalAccountId, LobbyIdString, MoveTemp(Result.GetErrorValue()));
		}
		else
		{
			// Attach the lobby details to the operation.
			InAsyncOp.Data.Set<TSharedRef<FLobbyDetailsEOS>>(LobbyDetailsKeyName, Result.GetOkValue());
			return MakeFulfilledPromise<void>().GetFuture();
		}
	})
	// Step 4: Create the lobby data object from the lobby details.
	.Then([this](TOnlineAsyncOp<FCreateLobby>& InAsyncOp)
	{
		TSharedRef<FLobbyDetailsEOS> LobbyDetails = GetOpDataChecked<TSharedRef<FLobbyDetailsEOS>>(InAsyncOp, LobbyDetailsKeyName);
		return LobbyDataRegistry->FindOrCreateFromLobbyDetails(InAsyncOp.GetParams().LocalAccountId, LobbyDetails);
	})
	// Step 5: Handle errors and store the lobby data on the async op properties.
	.Then([this, DestroyLobbyDuringCreate](TOnlineAsyncOp<FCreateLobby>& InAsyncOp, TDefaultErrorResultInternal<TSharedRef<FLobbyDataEOS>>&& Result)
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();
		const FString& LobbyIdString = GetOpDataChecked<FString>(InAsyncOp, LobbyIdStringKeyName);

		if (Result.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] FLobbyDataRegistryEOS::FindOrCreateFromLobbyDetails Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyIdString, *Result.GetErrorValue().GetLogString());

			return DestroyLobbyDuringCreate(InAsyncOp, Params.LocalAccountId, LobbyIdString, MoveTemp(Result.GetErrorValue()));
		}
		else
		{
			// Store lobby data on the operation.
			InAsyncOp.Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, MoveTemp(Result.GetOkValue()));
			return MakeFulfilledPromise<void>().GetFuture();
		}
	})
	// Step 6: Set lobby and creator attributes, change to user lobby privacy setting.
	.Then([this](TOnlineAsyncOp<FCreateLobby>& InAsyncOp)
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.Attributes = { Params.Attributes, {} };
		PrepareParams.ClientChanges.MemberAttributes = { Params.UserAttributes, {} };
		PrepareParams.ClientChanges.JoinPolicy = Params.JoinPolicy;
		PrepareParams.ClientChanges.LobbySchema = Params.SchemaId;
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(FLobbiesModifyLobbyDataImpl::Result{}).GetFuture();
		}

		FLobbiesModifyLobbyDataImpl::Params ModifyLobbyDataParams;
		ModifyLobbyDataParams.LobbyData = LobbyData;
		ModifyLobbyDataParams.LocalAccountId = Params.LocalAccountId;
		ModifyLobbyDataParams.ServiceChanges = MoveTemp(PrepareResult.GetOkValue().ServiceChanges);
		return ModifyLobbyDataImpl(MoveTemp(ModifyLobbyDataParams));
	})
	// Step 7: Handle result
	.Then([this, DestroyLobbyDuringCreate](TOnlineAsyncOp<FCreateLobby>& InAsyncOp, TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>&& Result) mutable
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		if (Result.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::CreateLobby] ModifyLobbyDataImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());

			return DestroyLobbyDuringCreate(InAsyncOp, Params.LocalAccountId, LobbyData->GetLobbyIdString(), MoveTemp(Result.GetErrorValue()));
		}
		else
		{
			return MakeFulfilledPromise<void>().GetFuture();
		}
	})
	// Step 8: Add the lobby to the active list, apply changes to the cached lobby object, and signal notifications.
	.Then([this](TOnlineAsyncOp<FCreateLobby>& InAsyncOp)
	{
		const FCreateLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Mark the lobby active.
		AddActiveLobby(Params.LocalAccountId, LobbyData);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Log, TEXT("[FLobbiesEOSGS::CreateLobby] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FCreateLobby::Result{LobbyData->GetLobbyClientData()->GetPublicDataPtr()});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FFindLobbies> FLobbiesEOSGS::FindLobbies(FFindLobbies::Params&& InParams)
{
	TOnlineAsyncOpRef<FFindLobbies> Op = GetOp<FFindLobbies>(MoveTemp(InParams));
	const FFindLobbies::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::FindLobbies] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	// Invalidate previous search results.
	ActiveSearchResults.Remove(Params.LocalAccountId);

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FFindLobbies>& InAsyncOp)
	{
		return FLobbySearchEOS::Create(LobbyPrerequisites.ToSharedRef(), LobbyDataRegistry.ToSharedRef(), InAsyncOp.GetParams());
	})
	.Then([this](TOnlineAsyncOp<FFindLobbies>& InAsyncOp, TDefaultErrorResultInternal<TSharedRef<FLobbySearchEOS>> Result)
	{
		if (Result.IsError())
		{
			const FFindLobbies::Params& Params = InAsyncOp.GetParams();

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] FLobbySearchEOS::Create Failed: User[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
		}
		else
		{
			InAsyncOp.Data.Set<TSharedRef<FLobbySearchEOS>>(LobbySearchKeyName, Result.GetOkValue());
		}
	})
	.Then([this](TOnlineAsyncOp<FFindLobbies>& InAsyncOp)
	{
		const FFindLobbies::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbySearchEOS>& LobbySearch = GetOpDataChecked<TSharedRef<FLobbySearchEOS>>(InAsyncOp, LobbySearchKeyName);

		ActiveSearchResults.Add(InAsyncOp.GetParams().LocalAccountId, LobbySearch);

		UE_LOG(LogTemp, Log, TEXT("[FLobbiesEOSGS::FindLobbies] Succeeded: User[%s]"),
			*ToLogString(Params.LocalAccountId));
		InAsyncOp.SetResult(FFindLobbies::Result{ LobbySearch->GetLobbyResults() });
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FJoinLobby> FLobbiesEOSGS::JoinLobby(FJoinLobby::Params&& InParams)
{
	TOnlineAsyncOpRef<FJoinLobby> Op = GetOp<FJoinLobby>(MoveTemp(InParams));
	const FJoinLobby::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	// Lobby data must already exist through local creation, invitation, search, or UI event when joining.
	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Setup lobby details - Prefer UI event before invitation before search result.
	TSharedPtr<FLobbyDetailsEOS> LobbyDetails = LobbyData->GetUserLobbyDetails(Params.LocalAccountId);
	if (!LobbyDetails)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] Failed: Unable to find lobby details for user. User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDetailsEOS>>(LobbyDetailsKeyName, LobbyDetails.ToSharedRef());

	// Check that lobby is compatible with the running game.
	if (!LobbyDetails->IsBucketCompatible())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] Failed: Lobby is not compatible with the client version. Lobby[%s], Bucket[%s], ExpectedBucket[%s]."),
			*LobbyData->GetLobbyIdString(), *ToLogString(LobbyDetails->GetInfo()->GetBucketId()), *ToLogString(LobbyPrerequisites->BucketId));
		Op->SetError(Errors::IncompatibleVersion());
		return Op->GetHandle();
	}

	// Start operation.
	// Step 1. Join the lobby.
	Op->Then([this](TOnlineAsyncOp<FJoinLobby>& InAsyncOp, TPromise<const EOS_Lobby_JoinLobbyCallbackInfo*>&& Promise)
	{
		const FJoinLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDetailsEOS>& LobbyDetails = GetOpDataChecked<TSharedRef<FLobbyDetailsEOS>>(InAsyncOp, LobbyDetailsKeyName);

		EOS_Lobby_JoinLobbyOptions JoinLobbyOptions = {};
		JoinLobbyOptions.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
		JoinLobbyOptions.LobbyDetailsHandle = LobbyDetails->GetEOSHandle();
		JoinLobbyOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
		JoinLobbyOptions.bPresenceEnabled = false; // todo
		JoinLobbyOptions.LocalRTCOptions = nullptr;
		static_assert(EOS_LOBBY_JOINLOBBY_API_LATEST == 3, "EOS_Lobby_JoinLobbyOptions updated, check new fields");

		EOS_Async(EOS_Lobby_JoinLobby, LobbyPrerequisites->LobbyInterfaceHandle, JoinLobbyOptions, MoveTemp(Promise));
	})
	// Step 2. Handle join result.
	.Then([this](TOnlineAsyncOp<FJoinLobby>& InAsyncOp, const EOS_Lobby_JoinLobbyCallbackInfo* Data)
	{
		if (Data->ResultCode != EOS_EResult::EOS_Success)
		{
			const FJoinLobby::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] EOS_Lobby_JoinLobby Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *LexToString(Data->ResultCode));
			InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
			return;
		}
	})
	// Step 3. Set member attributes.
	.Then([this](TOnlineAsyncOp<FJoinLobby>& InAsyncOp)
	{
		const FJoinLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.MemberAttributes = { Params.UserAttributes, {} };
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(FLobbiesModifyLobbyDataImpl::Result{}).GetFuture();
		}

		FLobbiesModifyLobbyDataImpl::Params ModifyLobbyDataParams;
		ModifyLobbyDataParams.LobbyData = LobbyData;
		ModifyLobbyDataParams.LocalAccountId = Params.LocalAccountId;
		ModifyLobbyDataParams.ServiceChanges = MoveTemp(PrepareResult.GetOkValue().ServiceChanges);
		return ModifyLobbyDataImpl(MoveTemp(ModifyLobbyDataParams));
	})
	// Step 4. Handle result.
	.Then([this](TOnlineAsyncOp<FJoinLobby>& InAsyncOp, TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FJoinLobby::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] ModifyLobbyMemberDataImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());

			// Failed to set attributes - leave the lobby.
			FLobbiesLeaveLobbyImpl::Params LeaveLobbyParams;
			LeaveLobbyParams.LobbyData = LobbyData;
			LeaveLobbyParams.LocalAccountId = Params.LocalAccountId;

			TPromise<void> Promise;
			TFuture<void> Future = Promise.GetFuture();

			LeaveLobbyImpl(MoveTemp(LeaveLobbyParams))
			.Next(
			[
				LocalAccountId = Params.LocalAccountId,
				LobbyData,
				InAsyncOp = InAsyncOp.AsShared(),
				ErrorResult = MoveTemp(Result.GetErrorValue()),
				Promise = MoveTemp(Promise)
			](TDefaultErrorResult<FLobbiesLeaveLobbyImpl>&& Result) mutable
			{
				if (Result.IsError())
				{
					UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::JoinLobby] LeaveLobbyImpl Failed: User[%s], Lobby[%s], Result[%s]"),
						*ToLogString(LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
				}

				InAsyncOp->SetError(MoveTemp(ErrorResult));
				Promise.EmplaceValue();
			});

			return Future;
		}
		else
		{
			return MakeFulfilledPromise<void>().GetFuture();
		}
	})
	// Step 5. Bookkeeping and notifications.
	.Then([this](TOnlineAsyncOp<FJoinLobby>& InAsyncOp)
	{
		const FJoinLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Mark the lobby active.
		AddActiveLobby(Params.LocalAccountId, LobbyData);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Log, TEXT("[FLobbiesEOSGS::JoinLobby] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FJoinLobby::Result{ LobbyData->GetLobbyClientData()->GetPublicDataPtr()});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FLeaveLobby> FLobbiesEOSGS::LeaveLobby(FLeaveLobby::Params&& InParams)
{
	TOnlineAsyncOpRef<FLeaveLobby> Op = GetOp<FLeaveLobby>(MoveTemp(InParams));
	const FLeaveLobby::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::LeaveLobby] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::LeaveLobby] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::LeaveLobby] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FLeaveLobby>& InAsyncOp)
	{
		const FLeaveLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.LocalUserLeaveReason = ELobbyMemberLeaveReason::Left;
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMember] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesLeaveLobbyImpl>>(FLobbiesLeaveLobbyImpl::Result{}).GetFuture();
		}

		FLobbiesLeaveLobbyImpl::Params LeaveParams;
		LeaveParams.LobbyData = LobbyData;
		LeaveParams.LocalAccountId = Params.LocalAccountId;
		return LeaveLobbyImpl(MoveTemp(LeaveParams));
	})
	.Then([this](TOnlineAsyncOp<FLeaveLobby>& InAsyncOp, TDefaultErrorResult<FLobbiesLeaveLobbyImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FLeaveLobby::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::LeaveLobby] LeaveLobbyImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());

			// When leaving a lobby consume the error and report success.
			//InAsyncOp.SetError(Result.GetErrorValue());

			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FLeaveLobby>& InAsyncOp)
	{
		const FLeaveLobby::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		// Remove the lobby from the active list for the user.
		// The lobby data will be cleaned up once all references are removed.
		RemoveActiveLobby(Params.LocalAccountId, LobbyData);

		UE_LOG(LogTemp, Log, TEXT("[FLobbiesEOSGS::LeaveLobby] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FLeaveLobby::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FInviteLobbyMember> FLobbiesEOSGS::InviteLobbyMember(FInviteLobbyMember::Params&& InParams)
{
	TOnlineAsyncOpRef<FInviteLobbyMember> Op = GetOp<FInviteLobbyMember>(MoveTemp(InParams));
	const FInviteLobbyMember::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMember] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMember] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMember] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FInviteLobbyMember>& InAsyncOp)
	{
		const FInviteLobbyMember::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbiesInviteLobbyMemberImpl::Params InviteParams;
		InviteParams.LobbyData = LobbyData;
		InviteParams.LocalAccountId = Params.LocalAccountId;
		InviteParams.TargetAccountId = Params.TargetAccountId;
		return InviteLobbyMemberImpl(MoveTemp(InviteParams));
	})
	.Then([this](TOnlineAsyncOp<FInviteLobbyMember>& InAsyncOp, TDefaultErrorResult<FLobbiesInviteLobbyMemberImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FInviteLobbyMember::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMember] InviteLobbyMemberImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FInviteLobbyMember>& InAsyncOp)
	{
		const FInviteLobbyMember::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::InviteLobbyMember] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FInviteLobbyMember::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FDeclineLobbyInvitation> FLobbiesEOSGS::DeclineLobbyInvitation(FDeclineLobbyInvitation::Params&& InParams)
{
	TOnlineAsyncOpRef<FDeclineLobbyInvitation> Op = GetOp<FDeclineLobbyInvitation>(MoveTemp(InParams));
	const FDeclineLobbyInvitation::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitation] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitation] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FDeclineLobbyInvitation>& InAsyncOp)
	{
		const FDeclineLobbyInvitation::Params& Params = InAsyncOp.GetParams();

		FLobbiesDeclineLobbyInvitationImpl::Params DeclineParams;
		DeclineParams.LocalAccountId = Params.LocalAccountId;
		DeclineParams.LobbyId = Params.LobbyId;
		return DeclineLobbyInvitationImpl(MoveTemp(DeclineParams));
	})
	.Then([this](TOnlineAsyncOp<FDeclineLobbyInvitation>& InAsyncOp, TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FDeclineLobbyInvitation::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitation] DeclineLobbyInvitationImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FDeclineLobbyInvitation>& InAsyncOp)
	{
		const FDeclineLobbyInvitation::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitation] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FDeclineLobbyInvitation::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FKickLobbyMember> FLobbiesEOSGS::KickLobbyMember(FKickLobbyMember::Params&& InParams)
{
	TOnlineAsyncOpRef<FKickLobbyMember> Op = GetOp<FKickLobbyMember>(MoveTemp(InParams));
	const FKickLobbyMember::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMember] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMember] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMember] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FKickLobbyMember>& InAsyncOp)
	{
		const FKickLobbyMember::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.KickedTargetMember = Params.TargetAccountId;
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMember] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>>(FLobbiesKickLobbyMemberImpl::Result{}).GetFuture();
		}

		FLobbiesKickLobbyMemberImpl::Params KickParams;
		KickParams.LobbyData = LobbyData;
		KickParams.LocalAccountId = Params.LocalAccountId;
		KickParams.TargetAccountId = *PrepareResult.GetOkValue().ServiceChanges.KickedTargetMember;
		return KickLobbyMemberImpl(MoveTemp(KickParams));
	})
	.Then([this](TOnlineAsyncOp<FKickLobbyMember>& InAsyncOp, TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FKickLobbyMember::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMember] KickLobbyMemberImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
		}
	})
	.Then([this](TOnlineAsyncOp<FKickLobbyMember>& InAsyncOp)
	{
		const FKickLobbyMember::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::KickLobbyMember] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FKickLobbyMember::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FPromoteLobbyMember> FLobbiesEOSGS::PromoteLobbyMember(FPromoteLobbyMember::Params&& InParams)
{
	// Check prerequisites.
	TOnlineAsyncOpRef<FPromoteLobbyMember> Op = GetOp<FPromoteLobbyMember>(MoveTemp(InParams));
	const FPromoteLobbyMember::Params& Params = Op->GetParams();

	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::FPromoteLobbyMember] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::FPromoteLobbyMember] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::FPromoteLobbyMember] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FPromoteLobbyMember>& InAsyncOp)
	{
		const FPromoteLobbyMember::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.OwnerAccountId = Params.TargetAccountId;
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::FPromoteLobbyMember] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>>(FLobbiesPromoteLobbyMemberImpl::Result{}).GetFuture();
		}

		FLobbiesPromoteLobbyMemberImpl::Params PromoteParams;
		PromoteParams.LobbyData = LobbyData;
		PromoteParams.LocalAccountId = Params.LocalAccountId;
		PromoteParams.TargetAccountId = *PrepareResult.GetOkValue().ServiceChanges.OwnerAccountId;
		return PromoteLobbyMemberImpl(MoveTemp(PromoteParams));
	})
	.Then([this](TOnlineAsyncOp<FPromoteLobbyMember>& InAsyncOp, TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FPromoteLobbyMember::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::FPromoteLobbyMember] PromoteLobbyMemberImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FPromoteLobbyMember>& InAsyncOp)
	{
		const FPromoteLobbyMember::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::FPromoteLobbyMember] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FPromoteLobbyMember::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FModifyLobbyJoinPolicy> FLobbiesEOSGS::ModifyLobbyJoinPolicy(FModifyLobbyJoinPolicy::Params&& InParams)
{
	TOnlineAsyncOpRef<FModifyLobbyJoinPolicy> Op = GetOp<FModifyLobbyJoinPolicy>(MoveTemp(InParams));
	const FModifyLobbyJoinPolicy::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyJoinPolicy] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyJoinPolicy] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyJoinPolicy] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FModifyLobbyJoinPolicy>& InAsyncOp)
	{
		const FModifyLobbyJoinPolicy::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.JoinPolicy = Params.JoinPolicy;
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyJoinPolicy] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(FLobbiesModifyLobbyDataImpl::Result{}).GetFuture();
		}

		FLobbiesModifyLobbyDataImpl::Params ModifyLobbyDataParams;
		ModifyLobbyDataParams.LobbyData = LobbyData;
		ModifyLobbyDataParams.LocalAccountId = Params.LocalAccountId;
		ModifyLobbyDataParams.ServiceChanges = MoveTemp(PrepareResult.GetOkValue().ServiceChanges);
		return ModifyLobbyDataImpl(MoveTemp(ModifyLobbyDataParams));
	})
	.Then([this](TOnlineAsyncOp<FModifyLobbyJoinPolicy>& InAsyncOp, TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FModifyLobbyJoinPolicy::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyJoinPolicy] ModifyLobbyDataImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FModifyLobbyJoinPolicy>& InAsyncOp)
	{
		const FModifyLobbyJoinPolicy::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::ModifyLobbyJoinPolicy] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FModifyLobbyJoinPolicy::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FModifyLobbyAttributes> FLobbiesEOSGS::ModifyLobbyAttributes(FModifyLobbyAttributes::Params&& InParams)
{
	TOnlineAsyncOpRef<FModifyLobbyAttributes> Op = GetOp<FModifyLobbyAttributes>(MoveTemp(InParams));
	const FModifyLobbyAttributes::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyAttributes] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyAttributes] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyAttributes] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FModifyLobbyAttributes>& InAsyncOp)
	{
		const FModifyLobbyAttributes::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.Attributes = { Params.UpdatedAttributes, Params.RemovedAttributes };
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyAttributes] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(FLobbiesModifyLobbyDataImpl::Result{}).GetFuture();
		}

		FLobbiesModifyLobbyDataImpl::Params ModifyLobbyDataParams;
		ModifyLobbyDataParams.LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
		ModifyLobbyDataParams.LocalAccountId = Params.LocalAccountId;
		ModifyLobbyDataParams.ServiceChanges = MoveTemp(PrepareResult.GetOkValue().ServiceChanges);
		return ModifyLobbyDataImpl(MoveTemp(ModifyLobbyDataParams));
	})
	.Then([this](TOnlineAsyncOp<FModifyLobbyAttributes>& InAsyncOp, TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FModifyLobbyAttributes::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyAttributes] ModifyLobbyDataImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FModifyLobbyAttributes>& InAsyncOp)
	{
		const FModifyLobbyAttributes::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::ModifyLobbyAttributes] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FModifyLobbyAttributes::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FModifyLobbyMemberAttributes> FLobbiesEOSGS::ModifyLobbyMemberAttributes(FModifyLobbyMemberAttributes::Params&& InParams)
{
	TOnlineAsyncOpRef<FModifyLobbyMemberAttributes> Op = GetOp<FModifyLobbyMemberAttributes>(MoveTemp(InParams));
	const FModifyLobbyMemberAttributes::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyMemberAttributes] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		Op->SetError(Errors::InvalidUser());
		return Op->GetHandle();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyMemberAttributes] Failed: Lobby id is invalid."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Params.LobbyId);
	if (!LobbyData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyMemberAttributes] Failed: Unable to find lobby data. LobbyId[%s]"), *ToLogString(Params.LobbyId));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}
	Op->Data.Set<TSharedRef<FLobbyDataEOS>>(LobbyDataKeyName, LobbyData.ToSharedRef());

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FModifyLobbyMemberAttributes>& InAsyncOp)
	{
		const FModifyLobbyMemberAttributes::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		FLobbyClientDataPrepareClientChanges::Params PrepareParams;
		PrepareParams.LocalAccountId = Params.LocalAccountId;
		PrepareParams.ClientChanges.MemberAttributes = { Params.UpdatedAttributes, Params.RemovedAttributes };
		TOnlineResult<FLobbyClientDataPrepareClientChanges> PrepareResult = LobbyData->GetLobbyClientData()->PrepareClientChanges(MoveTemp(PrepareParams));
		if (PrepareResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyMemberAttributes] PrepareClientChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *PrepareResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareResult.GetErrorValue()));
			return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(FLobbiesModifyLobbyDataImpl::Result{}).GetFuture();
		}

		FLobbiesModifyLobbyDataImpl::Params ModifyLobbyDataParams;
		ModifyLobbyDataParams.LobbyData = LobbyData;
		ModifyLobbyDataParams.LocalAccountId = Params.LocalAccountId;
		ModifyLobbyDataParams.ServiceChanges = MoveTemp(PrepareResult.GetOkValue().ServiceChanges);
		return ModifyLobbyDataImpl(MoveTemp(ModifyLobbyDataParams));
	})
	.Then([this](TOnlineAsyncOp<FModifyLobbyMemberAttributes>& InAsyncOp, TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>&& Result)
	{
		if (Result.IsError())
		{
			const FModifyLobbyMemberAttributes::Params& Params = InAsyncOp.GetParams();
			const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyMemberAttributes] ModifyLobbyMemberDataImpl Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(Result.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FModifyLobbyMemberAttributes>& InAsyncOp)
	{
		const FModifyLobbyMemberAttributes::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FLobbyDataEOS>& LobbyData = GetOpDataChecked<TSharedRef<FLobbyDataEOS>>(InAsyncOp, LobbyDataKeyName);

		// Update local cache and fire events.
		LobbyData->GetLobbyClientData()->CommitClientChanges({ &LobbyEvents });

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::ModifyLobbyMemberAttributes] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FModifyLobbyMemberAttributes::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineResult<FGetJoinedLobbies> FLobbiesEOSGS::GetJoinedLobbies(FGetJoinedLobbies::Params&& Params)
{
	if (const TSet<TSharedRef<FLobbyDataEOS>>* Lobbies = ActiveLobbies.Find(Params.LocalAccountId))
	{
		FGetJoinedLobbies::Result Result;
		Result.Lobbies.Reserve(Lobbies->Num());
		for (const TSharedRef<FLobbyDataEOS>& LobbyDataEOS : *Lobbies)
		{
			Result.Lobbies.Emplace(LobbyDataEOS->GetLobbyClientData()->GetPublicDataPtr());
		}
		return TOnlineResult<FGetJoinedLobbies>(MoveTemp(Result));
	}
	else
	{
		return TOnlineResult<FGetJoinedLobbies>(Errors::InvalidUser());
	}
}

void FLobbiesEOSGS::HandleLobbyUpdated(const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* Data)
{
	if (TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Data->LobbyId))
	{
		FLobbiesProcessLobbyNotificationImpl::Params Params;
		Params.LobbyData = LobbyData;

		ProcessLobbyNotificationImplOp(MoveTemp(Params))
		.OnComplete([LobbyData](const TOnlineResult<FLobbiesProcessLobbyNotificationImpl>& Result)
		{
			if (Result.IsError())
			{
				// Todo: handle failure to update lobby from snapshot.
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleLobbyUpdated] ProcessLobbyNotificationImplOp Failed: Lobby[%s], Result[%s]"),
					*LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::HandleLobbyUpdated] Succeeded: Lobby[%s]"),
					*LobbyData->GetLobbyIdString());
			}
		});
	}
}

void FLobbiesEOSGS::HandleLobbyMemberUpdated(const EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo* Data)
{
	if (TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Data->LobbyId))
	{
		FLobbiesProcessLobbyNotificationImpl::Params Params;
		Params.LobbyData = LobbyData;
		Params.MutatedMembers.Add(Data->TargetUserId);

		ProcessLobbyNotificationImplOp(MoveTemp(Params))
		.OnComplete([LobbyData](const TOnlineResult<FLobbiesProcessLobbyNotificationImpl>& Result)
		{
			if (Result.IsError())
			{
				// Todo: handle failure to update lobby from snapshot.
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleLobbyMemberUpdated] ProcessLobbyNotificationImplOp Failed: Lobby[%s], Result[%s]"),
					*LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::HandleLobbyMemberUpdated] Succeeded: Lobby[%s]"),
					*LobbyData->GetLobbyIdString());
			}
		});
	}
}

void FLobbiesEOSGS::HandleLobbyMemberStatusReceived(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* Data)
{
	if (TSharedPtr<FLobbyDataEOS> LobbyData = LobbyDataRegistry->Find(Data->LobbyId))
	{
		FLobbiesProcessLobbyNotificationImpl::Params Params;
		Params.LobbyData = LobbyData;

		switch (Data->CurrentStatus)
		{
		case EOS_ELobbyMemberStatus::EOS_LMS_JOINED:
			// Fetch member snapshot on join.
			Params.MutatedMembers.Add(Data->TargetUserId);
			break;

		case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED:
			// No member data needed, only lobby snapshot.
			break;

		case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:
			Params.LeavingMembers.Add(Data->TargetUserId, ELobbyMemberLeaveReason::Left);
			break;

		case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED:
			Params.LeavingMembers.Add(Data->TargetUserId, ELobbyMemberLeaveReason::Disconnected);
			break;

		case EOS_ELobbyMemberStatus::EOS_LMS_KICKED:
			Params.LeavingMembers.Add(Data->TargetUserId, ELobbyMemberLeaveReason::Kicked);
			break;

		default: checkNoEntry(); // Intentional fallthrough
		case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED:
			Params.LeavingMembers.Add(Data->TargetUserId, ELobbyMemberLeaveReason::Closed);
			break;
		}

		ProcessLobbyNotificationImplOp(MoveTemp(Params))
		.OnComplete([LobbyData](const TOnlineResult<FLobbiesProcessLobbyNotificationImpl>& Result)
		{
			if (Result.IsError())
			{
				// Todo: handle failure to update lobby from snapshot.
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleLobbyMemberStatusReceived] ProcessLobbyNotificationImplOp Failed: Lobby[%s], Result[%s]"),
					*LobbyData->GetLobbyIdString(), *Result.GetErrorValue().GetLogString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::HandleLobbyMemberStatusReceived] Succeeded: Lobby[%s]"),
					*LobbyData->GetLobbyIdString());
			}
		});
	}
}

void FLobbiesEOSGS::HandleLobbyInviteReceived(const EOS_Lobby_LobbyInviteReceivedCallbackInfo* Data)
{
	// Todo: Queue this like an operation.
	const FAccountId LocalAccountId = FindAccountId(Data->LocalUserId);
	if (LocalAccountId.IsValid())
	{
		FLobbyInviteDataEOS::CreateFromInviteId(LobbyPrerequisites.ToSharedRef(), LobbyDataRegistry.ToSharedRef(), LocalAccountId, Data->InviteId, Data->TargetUserId)
		.Next([this](TDefaultErrorResultInternal<TSharedRef<FLobbyInviteDataEOS>>&& Result)
		{
			if (Result.IsError())
			{
				// Todo: Log / queue a manual fetch of invitations.
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleLobbyInviteReceived] FLobbyInviteDataEOS::CreateFromInviteId Failed: Result[%s]"),
					*Result.GetErrorValue().GetLogString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::HandleLobbyMemberStatusReceived] Succeeded: InviteId[%s], Lobby[%s], Receiver[%s], Sender[%s]"),
					*Result.GetOkValue()->GetInviteId(),
					*Result.GetOkValue()->GetLobbyData()->GetLobbyIdString(),
					*ToLogString(Result.GetOkValue()->GetReceiver()),
					*ToLogString(Result.GetOkValue()->GetSender()));
				AddActiveInvite(Result.GetOkValue());
			}
		});
	}
}

void FLobbiesEOSGS::HandleLobbyInviteAccepted(const EOS_Lobby_LobbyInviteAcceptedCallbackInfo* Data)
{
	// Todo: Queue this like an operation.
	const FAccountId LocalAccountId = FindAccountId(Data->LocalUserId);
	if (LocalAccountId.IsValid())
	{
		FLobbyInviteDataEOS::CreateFromInviteId(LobbyPrerequisites.ToSharedRef(), LobbyDataRegistry.ToSharedRef(), LocalAccountId, Data->InviteId, Data->TargetUserId)
		.Next([this](TDefaultErrorResultInternal<TSharedRef<FLobbyInviteDataEOS>>&& Result)
		{
			if (Result.IsError())
			{
				// Todo: Log / queue a manual fetch of invitations.
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleLobbyInviteAccepted] FLobbyInviteDataEOS::CreateFromInviteId Failed: Result[%s]"),
					*Result.GetErrorValue().GetLogString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::HandleLobbyInviteAccepted] Succeeded: InviteId[%s], Lobby[%s], Receiver[%s], Sender[%s]"),
					*Result.GetOkValue()->GetInviteId(),
					*Result.GetOkValue()->GetLobbyData()->GetLobbyIdString(),
					*ToLogString(Result.GetOkValue()->GetReceiver()),
					*ToLogString(Result.GetOkValue()->GetSender()));

				const TSharedRef<FLobbyInviteDataEOS>& Invite = Result.GetOkValue();
				LobbyEvents.OnUILobbyJoinRequested.Broadcast(
					FUILobbyJoinRequested{
						Invite->GetReceiver(),
						TResult<TSharedRef<const FLobby>, FOnlineError>(Invite->GetLobbyData()->GetLobbyClientData()->GetPublicDataPtr()),
						EUILobbyJoinRequestedSource::FromInvitation
					});
			}
		});
	}
}

void FLobbiesEOSGS::HandleJoinLobbyAccepted(const EOS_Lobby_JoinLobbyAcceptedCallbackInfo* Data)
{
	// Todo: Queue this like an operation.
	const FAccountId LocalAccountId = FindAccountId(Data->LocalUserId);
	if (LocalAccountId.IsValid())
	{
		TDefaultErrorResultInternal<TSharedRef<FLobbyDetailsEOS>> LobbyDetailsResult = 
			FLobbyDetailsEOS::CreateFromUiEventId(LobbyPrerequisites.ToSharedRef(), LocalAccountId, Data->UiEventId);
		if (LobbyDetailsResult.IsError())
		{
			// Todo: Log / queue a manual fetch of invitations.
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleJoinLobbyAccepted] FLobbyDetailsEOS::CreateFromUiEventId Failed: User[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *LobbyDetailsResult.GetErrorValue().GetLogString());
			return;
		}

		LobbyDataRegistry->FindOrCreateFromLobbyDetails(LocalAccountId, LobbyDetailsResult.GetOkValue())
		.Next([this, LocalAccountId](TDefaultErrorResultInternal<TSharedRef<FLobbyDataEOS>> && Result)
		{
			if (Result.IsError())
			{
				UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleJoinLobbyAccepted] FLobbyDataRegistryEOS::FindOrCreateFromLobbyDetails Failed: User[%s], Result[%s]"),
					*ToLogString(LocalAccountId), *Result.GetErrorValue().GetLogString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::HandleJoinLobbyAccepted] Succeeded: User[%s], Lobby[%s]"),
					*ToLogString(LocalAccountId), *Result.GetOkValue()->GetLobbyIdString());

				LobbyEvents.OnUILobbyJoinRequested.Broadcast(
					FUILobbyJoinRequested{
						LocalAccountId,
						TResult<TSharedRef<const FLobby>, FOnlineError>(Result.GetOkValue()->GetLobbyClientData()->GetPublicDataPtr()),
						EUILobbyJoinRequestedSource::Unspecified
					});
			}
		});
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::HandleJoinLobbyAccepted] Failed: Invalid local user."));
	}
}

#if !UE_BUILD_SHIPPING
void FLobbiesEOSGS::CheckMetadata()
{
	// Metadata sanity check.
	ToLogString(FLobbiesLeaveLobbyImpl::Params());
	ToLogString(FLobbiesLeaveLobbyImpl::Result());
	ToLogString(FLobbiesDestroyLobbyImpl::Params());
	ToLogString(FLobbiesDestroyLobbyImpl::Result());
	ToLogString(FLobbiesInviteLobbyMemberImpl::Params());
	ToLogString(FLobbiesInviteLobbyMemberImpl::Result());
	ToLogString(FLobbiesDeclineLobbyInvitationImpl::Params());
	ToLogString(FLobbiesDeclineLobbyInvitationImpl::Result());
	ToLogString(FLobbiesKickLobbyMemberImpl::Params());
	ToLogString(FLobbiesKickLobbyMemberImpl::Result());
	ToLogString(FLobbiesPromoteLobbyMemberImpl::Params());
	ToLogString(FLobbiesPromoteLobbyMemberImpl::Result());
	ToLogString(FLobbiesModifyLobbyDataImpl::Params());
	ToLogString(FLobbiesModifyLobbyDataImpl::Result());
	ToLogString(FLobbiesProcessLobbyNotificationImpl::Params());
	ToLogString(FLobbiesProcessLobbyNotificationImpl::Result());
	Meta::VisitFields(FLobbiesLeaveLobbyImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesLeaveLobbyImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesDestroyLobbyImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesDestroyLobbyImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesInviteLobbyMemberImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesInviteLobbyMemberImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesDeclineLobbyInvitationImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesDeclineLobbyInvitationImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesKickLobbyMemberImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesKickLobbyMemberImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesPromoteLobbyMemberImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesPromoteLobbyMemberImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesModifyLobbyDataImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesModifyLobbyDataImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesProcessLobbyNotificationImpl::Params(), [](const TCHAR* Name, auto& Field) { return false; });
	Meta::VisitFields(FLobbiesProcessLobbyNotificationImpl::Result(), [](const TCHAR* Name, auto& Field) { return false; });
}
#endif

void FLobbiesEOSGS::RegisterHandlers()
{
	// Register for lobby updates.
	OnLobbyUpdatedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		LobbyPrerequisites->LobbyInterfaceHandle,
		EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST,
		&EOS_Lobby_AddNotifyLobbyUpdateReceived,
		&EOS_Lobby_RemoveNotifyLobbyUpdateReceived,
		&FLobbiesEOSGS::HandleLobbyUpdated);
	static_assert(EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST == 1, "EOS_Lobby_AddNotifyLobbyUpdateReceived updated, check new fields");

	// Register for lobby member updates.
	OnLobbyMemberUpdatedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		LobbyPrerequisites->LobbyInterfaceHandle,
		EOS_LOBBY_ADDNOTIFYLOBBYMEMBERUPDATERECEIVED_API_LATEST,
		&EOS_Lobby_AddNotifyLobbyMemberUpdateReceived,
		&EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived,
		&FLobbiesEOSGS::HandleLobbyMemberUpdated);
	static_assert(EOS_LOBBY_ADDNOTIFYLOBBYMEMBERUPDATERECEIVED_API_LATEST == 1, "EOS_Lobby_AddNotifyLobbyMemberUpdateReceived updated, check new fields");

	// Register for lobby member status changed.
	OnLobbyMemberStatusReceivedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		LobbyPrerequisites->LobbyInterfaceHandle,
		EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST,
		&EOS_Lobby_AddNotifyLobbyMemberStatusReceived,
		&EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived,
		&FLobbiesEOSGS::HandleLobbyMemberStatusReceived);
	static_assert(EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST == 1, "EOS_Lobby_AddNotifyLobbyMemberStatusReceived updated, check new fields");

	// Register for lobby invite received.
	OnLobbyInviteReceivedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		LobbyPrerequisites->LobbyInterfaceHandle,
		EOS_LOBBY_ADDNOTIFYLOBBYINVITERECEIVED_API_LATEST,
		&EOS_Lobby_AddNotifyLobbyInviteReceived,
		&EOS_Lobby_RemoveNotifyLobbyInviteReceived,
		&FLobbiesEOSGS::HandleLobbyInviteReceived);
	static_assert(EOS_LOBBY_ADDNOTIFYLOBBYINVITERECEIVED_API_LATEST == 1, "EOS_Lobby_AddNotifyLobbyInviteReceived updated, check new fields");

	// Register for lobby invite accepted.
	OnLobbyInviteAcceptedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		LobbyPrerequisites->LobbyInterfaceHandle,
		EOS_LOBBY_ADDNOTIFYLOBBYINVITEACCEPTED_API_LATEST,
		&EOS_Lobby_AddNotifyLobbyInviteAccepted,
		&EOS_Lobby_RemoveNotifyLobbyInviteAccepted,
		&FLobbiesEOSGS::HandleLobbyInviteAccepted);
	static_assert(EOS_LOBBY_ADDNOTIFYLOBBYINVITEACCEPTED_API_LATEST == 1, "EOS_Lobby_AddNotifyLobbyInviteAccepted updated, check new fields");

	// Register for join lobby accepted via overlay.
	OnJoinLobbyAcceptedEOSEventRegistration = EOS_RegisterComponentEventHandler(
		this,
		LobbyPrerequisites->LobbyInterfaceHandle,
		EOS_LOBBY_ADDNOTIFYJOINLOBBYACCEPTED_API_LATEST,
		&EOS_Lobby_AddNotifyJoinLobbyAccepted,
		&EOS_Lobby_RemoveNotifyJoinLobbyAccepted,
		&FLobbiesEOSGS::HandleJoinLobbyAccepted);
	static_assert(EOS_LOBBY_ADDNOTIFYJOINLOBBYACCEPTED_API_LATEST == 1, "EOS_Lobby_AddNotifyJoinLobbyAccepted updated, check new fields");
}

void FLobbiesEOSGS::UnregisterHandlers()
{
	OnLobbyUpdatedEOSEventRegistration = nullptr;
	OnLobbyMemberUpdatedEOSEventRegistration = nullptr;
	OnLobbyMemberStatusReceivedEOSEventRegistration = nullptr;
	OnLobbyInviteReceivedEOSEventRegistration = nullptr;
	OnLobbyInviteAcceptedEOSEventRegistration = nullptr;
	OnJoinLobbyAcceptedEOSEventRegistration = nullptr;
}

void FLobbiesEOSGS::AddActiveLobby(FAccountId LocalAccountId, const TSharedRef<FLobbyDataEOS>& LobbyData)
{
	// Add bookkeeping for the user.
	ActiveLobbies.FindOrAdd(LocalAccountId).Add(LobbyData);
}

void FLobbiesEOSGS::RemoveActiveLobby(FAccountId LocalAccountId, const TSharedRef<FLobbyDataEOS>& LobbyData)
{
	// Remove bookkeeping for the local user.
	if (TSet<TSharedRef<FLobbyDataEOS>>* Lobbies = ActiveLobbies.Find(LocalAccountId))
	{
		Lobbies->Remove(LobbyData);
	}
}

void FLobbiesEOSGS::AddActiveInvite(const TSharedRef<FLobbyInviteDataEOS>& Invite)
{
	TMap<FLobbyId, TSharedRef<FLobbyInviteDataEOS>>& ActiveUserInvites = ActiveInvites.FindOrAdd(Invite->GetReceiver());
	const FLobbyId LobbyId = Invite->GetLobbyData()->GetLobbyIdHandle();

	// Todo: Handle multiple invites for the same lobby.
	if (ActiveUserInvites.Find(LobbyId) == nullptr)
	{
		ActiveUserInvites.Add(LobbyId, Invite);
		LobbyEvents.OnLobbyInvitationAdded.Broadcast(
			FLobbyInvitationAdded{
				Invite->GetReceiver(),
				Invite->GetSender(),
				Invite->GetLobbyData()->GetLobbyClientData()->GetPublicDataPtr()
			});
	}
}

void FLobbiesEOSGS::RemoveActiveInvite(const TSharedRef<FLobbyInviteDataEOS>& Invite)
{
	ActiveInvites.FindOrAdd(Invite->GetReceiver()).Remove(Invite->GetLobbyData()->GetLobbyIdHandle());

	LobbyEvents.OnLobbyInvitationRemoved.Broadcast(
		FLobbyInvitationRemoved{
			Invite->GetReceiver(),
			Invite->GetSender(),
			Invite->GetLobbyData()->GetLobbyClientData()->GetPublicDataPtr()
		});
}

TSharedPtr<FLobbyInviteDataEOS> FLobbiesEOSGS::GetActiveInvite(FAccountId TargetUser, FLobbyId TargetLobbyId)
{
	const TSharedRef<FLobbyInviteDataEOS>* Result = ActiveInvites.FindOrAdd(TargetUser).Find(TargetLobbyId);
	return Result ? *Result : TSharedPtr<FLobbyInviteDataEOS>();
}

TFuture<TDefaultErrorResult<FLobbiesLeaveLobbyImpl>> FLobbiesEOSGS::LeaveLobbyImpl(FLobbiesLeaveLobbyImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::LeaveLobbyImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesLeaveLobbyImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (!Params.LobbyData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::LeaveLobbyImpl] Failed: No lobby data provided."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesLeaveLobbyImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	EOS_Lobby_LeaveLobbyOptions LeaveLobbyOptions = {};
	LeaveLobbyOptions.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
	LeaveLobbyOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	LeaveLobbyOptions.LobbyId = Params.LobbyData->GetLobbyIdEOS();
	static_assert(EOS_LOBBY_LEAVELOBBY_API_LATEST == 1, "EOS_Lobby_LeaveLobbyOptions updated, check new fields");

	TPromise<TDefaultErrorResult<FLobbiesLeaveLobbyImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesLeaveLobbyImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Lobby_LeaveLobby, LobbyPrerequisites->LobbyInterfaceHandle, LeaveLobbyOptions,
	[
		LocalAccountId = MoveTemp(Params.LocalAccountId),
		LobbyData = MoveTemp(Params.LobbyData),
		Promise = MoveTemp(Promise)
	](const EOS_Lobby_LeaveLobbyCallbackInfo* Result) mutable
	{
		if (Result->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DestroyLobbyImpl] EOS_Lobby_DestroyLobby Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *LobbyData->GetLobbyIdString(), *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::DestroyLobbyImpl] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *LobbyData->GetLobbyIdString());
		Promise.EmplaceValue(FLobbiesLeaveLobbyImpl::Result{});
	});

	return Future;
}

TFuture<TDefaultErrorResult<FLobbiesDestroyLobbyImpl>> FLobbiesEOSGS::DestroyLobbyImpl(FLobbiesDestroyLobbyImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DestroyLobbyImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesDestroyLobbyImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (Params.LobbyIdString.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DestroyLobbyImpl] Failed: No lobby id provided."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesDestroyLobbyImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	FTCHARToUTF8 LobbyIdTranslator(*Params.LobbyIdString);
	EOS_Lobby_DestroyLobbyOptions DestroyLobbyOptions = {};
	DestroyLobbyOptions.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
	DestroyLobbyOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	DestroyLobbyOptions.LobbyId = LobbyIdTranslator.Get();
	static_assert(EOS_LOBBY_DESTROYLOBBY_API_LATEST == 1, "EOS_Lobby_DestroyLobbyOptions updated, check new fields");

	TPromise<TDefaultErrorResult<FLobbiesDestroyLobbyImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesDestroyLobbyImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Lobby_DestroyLobby, LobbyPrerequisites->LobbyInterfaceHandle, DestroyLobbyOptions,
	[
		LocalAccountId = MoveTemp(Params.LocalAccountId),
		LobbyIdString = MoveTemp(Params.LobbyIdString),
		Promise = MoveTemp(Promise)
	](const EOS_Lobby_DestroyLobbyCallbackInfo* Result) mutable
	{
		if (Result->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DestroyLobbyImpl] EOS_Lobby_DestroyLobby Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *LobbyIdString, *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::DestroyLobbyImpl] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *LobbyIdString);
		Promise.EmplaceValue(FLobbiesDestroyLobbyImpl::Result{});
	});

	return Future;
}

TFuture<TDefaultErrorResult<FLobbiesInviteLobbyMemberImpl>> FLobbiesEOSGS::InviteLobbyMemberImpl(FLobbiesInviteLobbyMemberImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMemberImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesInviteLobbyMemberImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (!Params.LobbyData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMemberImpl] Failed: No lobby data provided."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesInviteLobbyMemberImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	EOS_Lobby_SendInviteOptions SendInviteOptions = {};
	SendInviteOptions.ApiVersion = EOS_LOBBY_SENDINVITE_API_LATEST;
	SendInviteOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	SendInviteOptions.TargetUserId = GetProductUserIdChecked(Params.TargetAccountId);
	SendInviteOptions.LobbyId = Params.LobbyData->GetLobbyIdEOS();
	static_assert(EOS_LOBBY_SENDINVITE_API_LATEST == 1, "EOS_Lobby_SendInviteOptions updated, check new fields");

	TPromise<TDefaultErrorResult<FLobbiesInviteLobbyMemberImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesInviteLobbyMemberImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Lobby_SendInvite, LobbyPrerequisites->LobbyInterfaceHandle, SendInviteOptions,
	[
		LocalAccountId = MoveTemp(Params.LocalAccountId),
		TargetAccountId = MoveTemp(Params.TargetAccountId),
		LobbyData = MoveTemp(Params.LobbyData),
		Promise = MoveTemp(Promise)
	](const EOS_Lobby_SendInviteCallbackInfo* Result) mutable
	{
		if (Result->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::InviteLobbyMemberImpl] EOS_Lobby_SendInvite Failed: User[%s], TargetUser[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *ToLogString(TargetAccountId), *LobbyData->GetLobbyIdString(), *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::InviteLobbyMemberImpl] Succeeded: User[%s], TargetUser[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *ToLogString(TargetAccountId), *LobbyData->GetLobbyIdString());
		Promise.EmplaceValue(FLobbiesInviteLobbyMemberImpl::Result{});
	});

	return Future;
}

TFuture<TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>> FLobbiesEOSGS::DeclineLobbyInvitationImpl(FLobbiesDeclineLobbyInvitationImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitationImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (!Params.LobbyId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitationImpl] Failed: Lobby id is invalid."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Find the active invitation.
	TSharedPtr<FLobbyInviteDataEOS> InviteData = GetActiveInvite(Params.LocalAccountId, Params.LobbyId);
	if (!InviteData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitationImpl] Failed: Unable to find invitation for lobby. User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *ToLogString(Params.LobbyId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	EOS_Lobby_RejectInviteOptions RejectInviteOptions = {};
	RejectInviteOptions.ApiVersion = EOS_LOBBY_REJECTINVITE_API_LATEST;
	RejectInviteOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	RejectInviteOptions.InviteId = InviteData->GetInviteIdEOS();
	static_assert(EOS_LOBBY_REJECTINVITE_API_LATEST == 1, "EOS_Lobby_RejectInviteOptions updated, check new fields");

	TPromise<TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesDeclineLobbyInvitationImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Lobby_RejectInvite, LobbyPrerequisites->LobbyInterfaceHandle, RejectInviteOptions,
	[
		this,
		LocalAccountId = MoveTemp(Params.LocalAccountId),
		InviteData = MoveTemp(InviteData),
		Promise = MoveTemp(Promise)
	](const EOS_Lobby_RejectInviteCallbackInfo* Result) mutable
	{
		// Remove active invitation.
		RemoveActiveInvite(InviteData.ToSharedRef());

		if (Result->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitationImpl] EOS_Lobby_RejectInvite Failed: User[%s], Invitation[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *InviteData->GetInviteId(), *InviteData->GetLobbyData()->GetLobbyIdString(), *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::DeclineLobbyInvitationImpl] Succeeded: User[%s], Invitation[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *InviteData->GetInviteId(), *InviteData->GetLobbyData()->GetLobbyIdString());
		Promise.EmplaceValue(FLobbiesDeclineLobbyInvitationImpl::Result{});
	});

	return Future;
}

TFuture<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>> FLobbiesEOSGS::KickLobbyMemberImpl(FLobbiesKickLobbyMemberImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMemberImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (!Params.LobbyData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMemberImpl] Failed: No lobby data provided."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>>(Errors::InvalidParams()).GetFuture();
	}

	if (Params.LobbyData->GetLobbyClientData()->GetPublicData().OwnerAccountId != Params.LocalAccountId)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMemberImpl] Failed: User is not the lobby owner. User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *Params.LobbyData->GetLobbyIdString());
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	EOS_Lobby_KickMemberOptions KickMemberOptions = {};
	KickMemberOptions.ApiVersion = EOS_LOBBY_KICKMEMBER_API_LATEST;
	KickMemberOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	KickMemberOptions.TargetUserId = GetProductUserIdChecked(Params.TargetAccountId);
	KickMemberOptions.LobbyId = Params.LobbyData->GetLobbyIdEOS();
	static_assert(EOS_LOBBY_KICKMEMBER_API_LATEST == 1, "EOS_Lobby_KickMemberOptions updated, check new fields");

	TPromise<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesKickLobbyMemberImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Lobby_KickMember, LobbyPrerequisites->LobbyInterfaceHandle, KickMemberOptions,
	[
		LocalAccountId = MoveTemp(Params.LocalAccountId),
		TargetAccountId = MoveTemp(Params.TargetAccountId),
		LobbyData = MoveTemp(Params.LobbyData),
		Promise = MoveTemp(Promise)
	](const EOS_Lobby_KickMemberCallbackInfo* Result) mutable
	{
		if (Result->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::KickLobbyMemberImpl] EOS_Lobby_KickMember Failed: User[%s], TargetUser[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *ToLogString(TargetAccountId), *LobbyData->GetLobbyIdString(), *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::KickLobbyMemberImpl] Succeeded: User[%s], TargetUser[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *ToLogString(TargetAccountId), *LobbyData->GetLobbyIdString());
		Promise.EmplaceValue(FLobbiesKickLobbyMemberImpl::Result{});
	});

	return Future;
}

TFuture<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>> FLobbiesEOSGS::PromoteLobbyMemberImpl(FLobbiesPromoteLobbyMemberImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::PromoteLobbyMemberImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (!Params.LobbyData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::PromoteLobbyMemberImpl] Failed: No lobby data provided."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>>(Errors::InvalidParams()).GetFuture();
	}

	if (Params.LobbyData->GetLobbyClientData()->GetPublicData().OwnerAccountId != Params.LocalAccountId)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::PromoteLobbyMemberImpl] Failed: User is not the lobby owner. User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *Params.LobbyData->GetLobbyIdString());
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	EOS_Lobby_PromoteMemberOptions PromoteMemberOptions = {};
	PromoteMemberOptions.ApiVersion = EOS_LOBBY_PROMOTEMEMBER_API_LATEST;
	PromoteMemberOptions.LocalUserId = GetProductUserIdChecked(Params.LocalAccountId);
	PromoteMemberOptions.TargetUserId = GetProductUserIdChecked(Params.TargetAccountId);
	PromoteMemberOptions.LobbyId = Params.LobbyData->GetLobbyIdEOS();
	static_assert(EOS_LOBBY_PROMOTEMEMBER_API_LATEST == 1, "EOS_Lobby_PromoteMemberOptions updated, check new fields");

	TPromise<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesPromoteLobbyMemberImpl>> Future = Promise.GetFuture();

	EOS_Async(EOS_Lobby_PromoteMember, LobbyPrerequisites->LobbyInterfaceHandle, PromoteMemberOptions,
	[
		LocalAccountId = MoveTemp(Params.LocalAccountId),
		TargetAccountId = MoveTemp(Params.TargetAccountId),
		LobbyData = MoveTemp(Params.LobbyData),
		Promise = MoveTemp(Promise)
	](const EOS_Lobby_PromoteMemberCallbackInfo* Result) mutable
	{
		if (Result->ResultCode != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::PromoteLobbyMemberImpl] EOS_Lobby_PromoteMember Failed: User[%s], TargetUser[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *ToLogString(TargetAccountId), *LobbyData->GetLobbyIdString(), *LexToString(Result->ResultCode));
			Promise.EmplaceValue(Errors::FromEOSResult(Result->ResultCode));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::PromoteLobbyMemberImpl] Succeeded: User[%s], TargetUser[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *ToLogString(TargetAccountId), *LobbyData->GetLobbyIdString());
		Promise.EmplaceValue(FLobbiesPromoteLobbyMemberImpl::Result{});
	});

	return Future;
}

TFuture<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>> FLobbiesEOSGS::ModifyLobbyDataImpl(FLobbiesModifyLobbyDataImpl::Params&& Params)
{
	// Check prerequisites.
	if (!Services.GetAuthInterface()->IsLoggedIn(Params.LocalAccountId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyDataImpl] Failed: User is not logged in. User[%s]"), *ToLogString(Params.LocalAccountId));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(Errors::InvalidUser()).GetFuture();
	}

	if (!Params.LobbyData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyDataImpl] Failed: No lobby data provided."));
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(Errors::InvalidParams()).GetFuture();
	}

	TSharedPtr<FLobbyDetailsEOS> LobbyDetails = Params.LobbyData->GetUserLobbyDetails(Params.LocalAccountId);
	if (!LobbyDetails)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyDataImpl] Failed: Unable to find lobby details for user. User[%s], Lobby[%s]"),
			*ToLogString(Params.LocalAccountId), *Params.LobbyData->GetLobbyIdString());
		return MakeFulfilledPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>>(Errors::InvalidParams()).GetFuture();
	}

	// Start operation.
	TPromise<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>> Promise;
	TFuture<TDefaultErrorResult<FLobbiesModifyLobbyDataImpl>> Future = Promise.GetFuture();

	LobbyDetails->ApplyLobbyDataUpdateFromLocalChanges(Params.LocalAccountId, Params.ServiceChanges)
	.Next([
		LocalAccountId = Params.LocalAccountId,
		LobbyData = Params.LobbyData,
		Promise = MoveTemp(Promise)
	](EOS_EResult EosResult) mutable
	{
		if (EosResult != EOS_EResult::EOS_Success && EosResult != EOS_EResult::EOS_NoChange)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ModifyLobbyDataImpl] ApplyLobbyDataUpdateFromLocalChanges Failed: User[%s], Lobby[%s], Result[%s]"),
				*ToLogString(LocalAccountId), *LobbyData->GetLobbyIdString(), *LexToString(EosResult));
			Promise.EmplaceValue(Errors::FromEOSResult(EosResult));
			return;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::ModifyLobbyDataImpl] Succeeded: User[%s], Lobby[%s]"),
			*ToLogString(LocalAccountId), *LobbyData->GetLobbyIdString());
		Promise.EmplaceValue(FLobbiesModifyLobbyDataImpl::Result{});
	});

	return Future;
}

TOnlineAsyncOpHandle<FLobbiesProcessLobbyNotificationImpl> FLobbiesEOSGS::ProcessLobbyNotificationImplOp(FLobbiesProcessLobbyNotificationImpl::Params&& InParams)
{
	TOnlineAsyncOpRef<FLobbiesProcessLobbyNotificationImpl> Op = GetOp<FLobbiesProcessLobbyNotificationImpl>(MoveTemp(InParams));
	const FLobbiesProcessLobbyNotificationImpl::Params& Params = Op->GetParams();

	// Check prerequisites.
	if (!Params.LobbyData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ProcessLobbyNotificationImplOp] Failed: No lobby data provided."));
		Op->SetError(Errors::InvalidParams());
		return Op->GetHandle();
	}

	// Start operation.
	Op->Then([this](TOnlineAsyncOp<FLobbiesProcessLobbyNotificationImpl>& InAsyncOp)
	{
		const FLobbiesProcessLobbyNotificationImpl::Params& Params = InAsyncOp.GetParams();

		// Notifications do not always indicate a user. Try to find a valid lobby details object to
		// handle acquiring data snapshots.
		TSharedPtr<FLobbyDetailsEOS> LobbyDetails = Params.LobbyData->GetActiveLobbyDetails();
		if (!LobbyDetails.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ProcessLobbyNotificationImplOp] Failed: Unable to find active lobby details to process notificaions. Lobby[%s]"),
				*Params.LobbyData->GetLobbyIdString());
			InAsyncOp.SetError(Errors::InvalidState());
			return MakeFulfilledPromise<TDefaultErrorResultInternal<FLobbyServiceSnapshot>>(Errors::InvalidState()).GetFuture();
		}

		InAsyncOp.Data.Set<TSharedRef<FLobbyDetailsEOS>>(LobbyDetailsKeyName, LobbyDetails.ToSharedRef());

		// Fetch lobby snapshot. Fetching the snapshot resolves the account ids of all lobby members in the snapshot.
		return LobbyDetails->GetLobbySnapshot();
	})
	.Then([this](TOnlineAsyncOp<FLobbiesProcessLobbyNotificationImpl>& InAsyncOp, TDefaultErrorResultInternal<FLobbyServiceSnapshot>&& LobbySnapshotResult)
	{
		const FLobbiesProcessLobbyNotificationImpl::Params& Params = InAsyncOp.GetParams();
		TSharedRef<FLobbyDetailsEOS> LobbyDetails = GetOpDataChecked<TSharedRef<FLobbyDetailsEOS>>(InAsyncOp, LobbyDetailsKeyName);

		if (LobbySnapshotResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ProcessLobbyNotificationImplOp] GetLobbySnapshot Failed. Lobby[%s], Result[%s]"),
				*Params.LobbyData->GetLobbyIdString(), *LobbySnapshotResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(LobbySnapshotResult.GetErrorValue()));
			return;
		}

		// Get member snapshots.
		TMap<FAccountId, FLobbyMemberServiceSnapshot> LobbyMemberSnapshots;
		LobbyMemberSnapshots.Reserve(Params.MutatedMembers.Num());
		for (EOS_ProductUserId MutatedMember : Params.MutatedMembers)
		{
			const FAccountId MutatedMemberAccountId = FindAccountId(MutatedMember);
			if (MutatedMemberAccountId.IsValid())
			{
				TDefaultErrorResultInternal<FLobbyMemberServiceSnapshot> MemberSnapshotResult = LobbyDetails->GetLobbyMemberSnapshot(MutatedMemberAccountId);
				if (MemberSnapshotResult.IsError())
				{
					UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ProcessLobbyNotificationImplOp] GetLobbyMemberSnapshot Failed. User[%s], Lobby[%s], Result[%s]"),
						*ToLogString(MutatedMemberAccountId), *Params.LobbyData->GetLobbyIdString(), *LobbySnapshotResult.GetErrorValue().GetLogString());
					InAsyncOp.SetError(MoveTemp(MemberSnapshotResult.GetErrorValue()));
					return;
				}
				LobbyMemberSnapshots.Add(MutatedMemberAccountId, MoveTemp(MemberSnapshotResult.GetOkValue()));
			}
		}

		// Translate leaving members from EOS_ProductUserId to FAccountId.
		TMap<FAccountId, ELobbyMemberLeaveReason> LeavingMemberReason;
		LeavingMemberReason.Reserve(Params.LeavingMembers.Num());
		for (const TPair<EOS_ProductUserId, ELobbyMemberLeaveReason>& LeavingMember : Params.LeavingMembers)
		{
			const FAccountId LeavingMemberAccountId = FindAccountId(LeavingMember.Key);
			if (LeavingMemberAccountId.IsValid())
			{
				LeavingMemberReason.Add(LeavingMemberAccountId, LeavingMember.Value);
			}
		}

		// Prepare client state from snapshot.
		TOnlineResult<FLobbyClientDataPrepareServiceSnapshot> PrepareSnapshotResult =
			Params.LobbyData->GetLobbyClientData()->PrepareServiceSnapshot({ MoveTemp(LobbySnapshotResult.GetOkValue()), MoveTemp(LobbyMemberSnapshots), MoveTemp(LeavingMemberReason) });
		if (PrepareSnapshotResult.IsError())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FLobbiesEOSGS::ProcessLobbyNotificationImplOp] PrepareServiceSnapshot Failed. Lobby[%s], Result[%s]"),
				*Params.LobbyData->GetLobbyIdString(), *PrepareSnapshotResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(MoveTemp(PrepareSnapshotResult.GetErrorValue()));
			return;
		}
	})
	.Then([this](TOnlineAsyncOp<FLobbiesProcessLobbyNotificationImpl>& InAsyncOp)
	{
		const FLobbiesProcessLobbyNotificationImpl::Params& Params = InAsyncOp.GetParams();

		// Apply updates and fire notifications.
		FLobbyClientDataCommitServiceSnapshot::Result Result = Params.LobbyData->GetLobbyClientData()->CommitServiceSnapshot({ &LobbyEvents });

		// Remove active users if needed.
		for (FAccountId LeavingMember : Result.LeavingLocalMembers)
		{
			RemoveActiveLobby(LeavingMember, Params.LobbyData.ToSharedRef());
		}

		UE_LOG(LogTemp, Verbose, TEXT("[FLobbiesEOSGS::ProcessLobbyNotificationImplOp] Succeeded: Lobby[%s]"),
			*Params.LobbyData->GetLobbyIdString());
		InAsyncOp.SetResult(FLobbiesProcessLobbyNotificationImpl::Result{});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

/* UE::Online */ }
