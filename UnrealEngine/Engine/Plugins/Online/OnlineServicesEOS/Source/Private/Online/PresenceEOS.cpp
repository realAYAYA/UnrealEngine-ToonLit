// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/PresenceEOS.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineIdEOS.h"
#include "Online/OnlineServicesEOS.h"
#include "Online/OnlineServicesEOSTypes.h"
#include "Online/AuthEOS.h"
#include "Online/OnlineErrorEOSGS.h"

#include "eos_presence.h"

namespace UE::Online {

static inline EUserPresenceStatus ToEPresenceState(EOS_Presence_EStatus InStatus)
{
	switch (InStatus)
	{
		case EOS_Presence_EStatus::EOS_PS_Online:
		{
			return EUserPresenceStatus::Online;
		}
		case EOS_Presence_EStatus::EOS_PS_Away:
		{
			return EUserPresenceStatus::Away;
		}
		case EOS_Presence_EStatus::EOS_PS_ExtendedAway:
		{
			return EUserPresenceStatus::ExtendedAway;
		}
		case EOS_Presence_EStatus::EOS_PS_DoNotDisturb:
		{
			return EUserPresenceStatus::DoNotDisturb;
		}
	}
	return EUserPresenceStatus::Offline;
}

static inline EOS_Presence_EStatus ToEOS_Presence_EStatus(EUserPresenceStatus InState)
{
	switch (InState)
	{
	case EUserPresenceStatus::Online:
	{
		return EOS_Presence_EStatus::EOS_PS_Online;
	}
	case EUserPresenceStatus::Away:
	{
		return EOS_Presence_EStatus::EOS_PS_Away;
	}
	case EUserPresenceStatus::ExtendedAway:
	{
		return EOS_Presence_EStatus::EOS_PS_ExtendedAway;
	}
	case EUserPresenceStatus::DoNotDisturb:
	{
		return EOS_Presence_EStatus::EOS_PS_DoNotDisturb;
	}
	}
	return EOS_Presence_EStatus::EOS_PS_Offline;
}

FPresenceEOS::FPresenceEOS(FOnlineServicesEOS& InServices)
	: FPresenceCommon(InServices)
{
}

void FPresenceEOS::Initialize()
{
	FPresenceCommon::Initialize();

	PresenceHandle = EOS_Platform_GetPresenceInterface(static_cast<FOnlineServicesEOS&>(GetServices()).GetEOSPlatformHandle());
	check(PresenceHandle != nullptr);

	// Register for friend updates
	EOS_Presence_AddNotifyOnPresenceChangedOptions Options = { };
	Options.ApiVersion = EOS_PRESENCE_ADDNOTIFYONPRESENCECHANGED_API_LATEST;
	NotifyPresenceChangedNotificationId = EOS_Presence_AddNotifyOnPresenceChanged(PresenceHandle, &Options, this, [](const EOS_Presence_PresenceChangedCallbackInfo* Data)
	{
		FPresenceEOS* This = reinterpret_cast<FPresenceEOS*>(Data->ClientData);
		const FAccountId LocalAccountId = FindAccountIdChecked(Data->LocalUserId);

		This->Services.Get<FAuthEOS>()->ResolveAccountId(LocalAccountId, Data->PresenceUserId)
		.Next([This, LocalAccountId](const FAccountId& PresenceAccountId)
		{
			UE_LOG(LogTemp, Warning, TEXT("OnEOSPresenceUpdate: LocalAccountId=[%s] PresenceAccountId=[%s]"), *ToLogString(LocalAccountId), *ToLogString(PresenceAccountId));
			This->UpdateUserPresence(LocalAccountId, PresenceAccountId);
		});
	});
}

void FPresenceEOS::PreShutdown()
{
	EOS_Presence_RemoveNotifyOnPresenceChanged(PresenceHandle, NotifyPresenceChangedNotificationId);
}

TOnlineAsyncOpHandle<FQueryPresence> FPresenceEOS::QueryPresence(FQueryPresence::Params&& InParams)
{
	TOnlineAsyncOpRef<FQueryPresence> Op = GetJoinableOp<FQueryPresence>(MoveTemp(InParams));
	if (!Op->IsReady())
	{
		// Initialize
		const FQueryPresence::Params& Params = Op->GetParams();
		if (!Services.Get<FAuthEOS>()->IsLoggedIn(Params.LocalAccountId))
		{
			Op->SetError(Errors::NotLoggedIn());
			return Op->GetHandle();
		}
		EOS_EpicAccountId TargetUserEasId = GetEpicAccountId(Params.TargetAccountId);
		if (!EOS_EpicAccountId_IsValid(TargetUserEasId))
		{
			Op->SetError(Errors::NotLoggedIn());
			return Op->GetHandle();
		}

		// TODO:  If we try to query a local user's presence, is that an error, should we return the cached state, should we still ask EOS?
		const bool bIsLocalUser = Services.Get<FAuthEOS>()->IsLoggedIn(Params.TargetAccountId);
		if (bIsLocalUser)
		{
			Op->SetError(Errors::CannotQueryLocalUsers()); 
			return Op->GetHandle();
		}

		Op->Then([this](TOnlineAsyncOp<FQueryPresence>& InAsyncOp, TPromise<const EOS_Presence_QueryPresenceCallbackInfo*>&& Promise)
			{
				const FQueryPresence::Params& Params = InAsyncOp.GetParams();
				EOS_Presence_QueryPresenceOptions QueryPresenceOptions = { };
				QueryPresenceOptions.ApiVersion = EOS_PRESENCE_QUERYPRESENCE_API_LATEST;
				QueryPresenceOptions.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
				QueryPresenceOptions.TargetUserId = GetEpicAccountIdChecked(Params.TargetAccountId);

				EOS_Async(EOS_Presence_QueryPresence, PresenceHandle, QueryPresenceOptions, MoveTemp(Promise));
			})
			.Then([this](TOnlineAsyncOp<FQueryPresence>& InAsyncOp, const EOS_Presence_QueryPresenceCallbackInfo* Data) mutable
			{
				UE_LOG(LogTemp, Warning, TEXT("QueryPresenceResult: [%s]"), *LexToString(Data->ResultCode));

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					const FQueryPresence::Params& Params = InAsyncOp.GetParams();
					UpdateUserPresence(Params.LocalAccountId, Params.TargetAccountId);
					FQueryPresence::Result Result = { FindOrCreatePresence(Params.LocalAccountId, Params.TargetAccountId) };
					InAsyncOp.SetResult(MoveTemp(Result));
				}
				else
				{
					InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
				}
			})
			.Enqueue(GetSerialQueue());
	}
	return Op->GetHandle();
}

TOnlineResult<FGetCachedPresence> FPresenceEOS::GetCachedPresence(FGetCachedPresence::Params&& Params)
{
	if (TMap<FAccountId, TSharedRef<FUserPresence>>* PresenceList = PresenceLists.Find(Params.LocalAccountId))
	{
		TSharedRef<FUserPresence>* PresencePtr = PresenceList->Find(Params.TargetAccountId);
		if (PresencePtr)
		{
			FGetCachedPresence::Result Result = { *PresencePtr };
			return TOnlineResult<FGetCachedPresence>(MoveTemp(Result));
		}
	}
	return TOnlineResult<FGetCachedPresence>(Errors::NotFound()); 
}


TOnlineAsyncOpHandle<FUpdatePresence> FPresenceEOS::UpdatePresence(FUpdatePresence::Params&& InParams)
{
	TOnlineAsyncOpRef<FUpdatePresence> Op = GetOp<FUpdatePresence>(MoveTemp(InParams));
	const FUpdatePresence::Params& Params = Op->GetParams();
	if (!Services.Get<FAuthEOS>()->IsLoggedIn(Params.LocalAccountId))
	{
		Op->SetError(Errors::NotLoggedIn());
		return Op->GetHandle();
	}

	Op->Then([this](TOnlineAsyncOp<FUpdatePresence>& InAsyncOp, TPromise<const EOS_Presence_SetPresenceCallbackInfo*>&& Promise) mutable
	{
		const FUpdatePresence::Params& Params = InAsyncOp.GetParams();
		EOS_HPresenceModification ChangeHandle = nullptr;
		EOS_Presence_CreatePresenceModificationOptions Options = { };
		Options.ApiVersion = EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST;
		Options.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
		EOS_EResult CreatePresenceModificationResult = EOS_Presence_CreatePresenceModification(PresenceHandle, &Options, &ChangeHandle);
		if (CreatePresenceModificationResult == EOS_EResult::EOS_Success)
		{
			check(ChangeHandle != nullptr);

			// State
			EOS_PresenceModification_SetStatusOptions StatusOptions = { };
			StatusOptions.ApiVersion = EOS_PRESENCEMODIFICATION_SETSTATUS_API_LATEST;
			StatusOptions.Status = ToEOS_Presence_EStatus(Params.Presence->Status);
			EOS_EResult SetStatusResult = EOS_PresenceModification_SetStatus(ChangeHandle, &StatusOptions);
			if (SetStatusResult != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_SetStatus failed with result %s"), *LexToString(SetStatusResult));
				InAsyncOp.SetError(Errors::FromEOSResult(SetStatusResult));
				Promise.EmplaceValue();
				return;
			}

			// Raw rich text
			// Convert the status string as the rich text string
			EOS_PresenceModification_SetRawRichTextOptions RawRichTextOptions = { };
			RawRichTextOptions.ApiVersion = EOS_PRESENCEMODIFICATION_SETRAWRICHTEXT_API_LATEST;

			const FTCHARToUTF8 Utf8RawRichText(*Params.Presence->StatusString);
			RawRichTextOptions.RichText = Utf8RawRichText.Get();

			EOS_EResult SetRawRichTextResult = EOS_PresenceModification_SetRawRichText(ChangeHandle, &RawRichTextOptions);
			if (SetRawRichTextResult != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_SetRawRichText failed with result %s"), *LexToString(SetRawRichTextResult));
				InAsyncOp.SetError(Errors::FromEOSResult(SetRawRichTextResult));
				Promise.EmplaceValue();
				return;
			}

			// EOS needs to be specific on which fields are removed and which aren't, so grab the last presence and check which are deletions
			TArray<FString> RemovedProperties;
			TSharedRef<FUserPresence> LastUserPresence = FindOrCreatePresence(Params.LocalAccountId, Params.LocalAccountId);
			for (const TPair<FString, FPresenceVariant>& Pair : LastUserPresence->Properties)
			{
				if(!Params.Presence->Properties.Contains(Pair.Key))
				{
					RemovedProperties.Add(Pair.Key);
				}
			}

			// Removed fields
			if (RemovedProperties.Num() > 0)
			{
				// EOS_PresenceModification_DeleteData
				TArray<FTCHARToUTF8, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> Utf8Strings;
				TArray<EOS_PresenceModification_DataRecordId, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> RecordIds;
				int32 CurrentIndex = 0;
				for (const FString& RemovedProperty : RemovedProperties)
				{
					const FTCHARToUTF8& Utf8Key = Utf8Strings.Emplace_GetRef(*RemovedProperty);

					EOS_PresenceModification_DataRecordId& RecordId = RecordIds.Emplace_GetRef();
					RecordId.ApiVersion = EOS_PRESENCEMODIFICATION_DATARECORDID_API_LATEST;
					RecordId.Key = Utf8Key.Get();

					UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Removing field %s"), *RemovedProperty); // Temp logging
				}

				EOS_PresenceModification_DeleteDataOptions DataOptions = { };
				DataOptions.ApiVersion = EOS_PRESENCEMODIFICATION_DELETEDATA_API_LATEST;
				DataOptions.RecordsCount = RecordIds.Num();
				DataOptions.Records = RecordIds.GetData();
				EOS_EResult DeleteDataResult = EOS_PresenceModification_DeleteData(ChangeHandle, &DataOptions);
				if (DeleteDataResult != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_DeleteDataOptions failed with result %s"), *LexToString(DeleteDataResult));
					InAsyncOp.SetError(Errors::FromEOSResult(DeleteDataResult));
					Promise.EmplaceValue();
					return;
				}
			}

			// Added/Updated fields
			if (Params.Presence->Properties.Num() > 0)
			{
				if (Params.Presence->Properties.Num() > EOS_PRESENCE_DATA_MAX_KEYS)
				{
					// TODO: Move this check higher.  Needs to take into account number of present fields (not just ones updated) and removed fields.
					UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Too many presence keys.  %u/%u"), Params.Presence->Properties.Num(), EOS_PRESENCE_DATA_MAX_KEYS);
					InAsyncOp.SetError(Errors::InvalidParams());
					Promise.EmplaceValue();
					return;
				}
				TArray<FTCHARToUTF8, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS * 2>> Utf8Strings;
				TArray<EOS_Presence_DataRecord, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> Records;
				int32 CurrentIndex = 0;
				for (const TPair<FString, FPresenceVariant>& UpdatedProperty : Params.Presence->Properties)
				{
					const FTCHARToUTF8& Utf8Key = Utf8Strings.Emplace_GetRef(*UpdatedProperty.Key);
					const FTCHARToUTF8& Utf8Value = Utf8Strings.Emplace_GetRef(*UpdatedProperty.Value); // TODO: Better serialization

					EOS_Presence_DataRecord& Record = Records.Emplace_GetRef();
					Record.ApiVersion = EOS_PRESENCE_DATARECORD_API_LATEST;
					Record.Key = Utf8Key.Get();
					Record.Value = Utf8Value.Get();
					UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Set field [%s] to [%s]"), *UpdatedProperty.Key, *UpdatedProperty.Value); // Temp logging
				}

				EOS_PresenceModification_SetDataOptions DataOptions = { };
				DataOptions.ApiVersion = EOS_PRESENCEMODIFICATION_SETDATA_API_LATEST;
				DataOptions.RecordsCount = Records.Num();
				DataOptions.Records = Records.GetData();
				EOS_EResult SetDataResult = EOS_PresenceModification_SetData(ChangeHandle, &DataOptions);
				if (SetDataResult != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_SetData failed with result %s"), *LexToString(SetDataResult));
					InAsyncOp.SetError(Errors::FromEOSResult(SetDataResult));
					Promise.EmplaceValue();
					return;
				}
			}

			EOS_Presence_SetPresenceOptions SetPresenceOptions = { };
			SetPresenceOptions.ApiVersion = EOS_PRESENCE_SETPRESENCE_API_LATEST;
			SetPresenceOptions.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
			SetPresenceOptions.PresenceModificationHandle = ChangeHandle;

			EOS_Async(EOS_Presence_SetPresence, PresenceHandle, SetPresenceOptions, MoveTemp(Promise));
			EOS_PresenceModification_Release(ChangeHandle);
			return;
		}
		else
		{
			InAsyncOp.SetError(Errors::FromEOSResult(CreatePresenceModificationResult));
		}
		Promise.EmplaceValue();
	})
	.Then([this](TOnlineAsyncOp<FUpdatePresence>& InAsyncOp, const EOS_Presence_SetPresenceCallbackInfo* Data) mutable
	{
		UE_LOG(LogTemp, Warning, TEXT("SetPresenceResult: [%s]"), *LexToString(Data->ResultCode));

		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			// Update local presence
			const FUpdatePresence::Params& Params = InAsyncOp.GetParams();
			TSharedRef<FUserPresence> LocalUserPresence = FindOrCreatePresence(Params.LocalAccountId, Params.LocalAccountId);
			*LocalUserPresence = *Params.Presence;

			InAsyncOp.SetResult(FUpdatePresence::Result());
		}
		else
		{
			InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
		}
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FPartialUpdatePresence> FPresenceEOS::PartialUpdatePresence(FPartialUpdatePresence::Params&& InParams)
{
	// TODO: Validate params
	// EOS_PRESENCE_DATA_MAX_KEYS - number of total keys.  Compare proposed with existing with pending ops?  Include removed keys!
	// EOS_PRESENCE_DATA_MAX_KEY_LENGTH - length of each key.  Compare updated with this.
	// EOS_PRESENCE_DATA_MAX_VALUE_LENGTH - length of each value.  Compare updated with this.
	// EOS_PRESENCE_RICH_TEXT_MAX_VALUE_LENGTH - length of status. Compare updated with this.

	TOnlineAsyncOpRef<FPartialUpdatePresence> Op = GetMergeableOp<FPartialUpdatePresence>(MoveTemp(InParams));
	if (!Op->IsReady())
	{
		// Initialize
		const FPartialUpdatePresence::Params& Params = Op->GetParams();
		if (!Services.Get<FAuthEOS>()->IsLoggedIn(Params.LocalAccountId))
		{
			Op->SetError(Errors::NotLoggedIn());
			return Op->GetHandle();
		}

		// Don't cache anything from Params as they could be modified by another merge in the meanwhile.
		Op->Then([this](TOnlineAsyncOp<FPartialUpdatePresence>& InAsyncOp, TPromise<const EOS_Presence_SetPresenceCallbackInfo*>&& Promise) mutable
			{
				const FPartialUpdatePresence::Params& Params = InAsyncOp.GetParams();
				EOS_HPresenceModification ChangeHandle = nullptr;
				EOS_Presence_CreatePresenceModificationOptions Options = { };
				Options.ApiVersion = EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST;
				Options.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
				EOS_EResult CreatePresenceModificationResult = EOS_Presence_CreatePresenceModification(PresenceHandle, &Options, &ChangeHandle);
				if (CreatePresenceModificationResult == EOS_EResult::EOS_Success)
				{
					check(ChangeHandle != nullptr);

					// State
					if (Params.Mutations.Status.IsSet())
					{
						EOS_PresenceModification_SetStatusOptions StatusOptions = { };
						StatusOptions.ApiVersion = EOS_PRESENCEMODIFICATION_SETSTATUS_API_LATEST;
						StatusOptions.Status = ToEOS_Presence_EStatus(Params.Mutations.Status.GetValue());
						EOS_EResult SetStatusResult = EOS_PresenceModification_SetStatus(ChangeHandle, &StatusOptions);
						if (SetStatusResult != EOS_EResult::EOS_Success)
						{
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_SetStatus failed with result %s"), *LexToString(SetStatusResult));
							InAsyncOp.SetError(Errors::FromEOSResult(SetStatusResult));
							Promise.EmplaceValue();
							return;
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Status set to %u"), (uint8)Params.Mutations.Status.GetValue()); // Temp logging
						}
					}

					// Raw rich text
					if (Params.Mutations.StatusString.IsSet())
					{
						// Convert the status string as the rich text string
						EOS_PresenceModification_SetRawRichTextOptions RawRichTextOptions = { };
						RawRichTextOptions.ApiVersion = EOS_PRESENCEMODIFICATION_SETRAWRICHTEXT_API_LATEST;

						const FTCHARToUTF8 Utf8RawRichText(*Params.Mutations.StatusString.GetValue());
						RawRichTextOptions.RichText = Utf8RawRichText.Get();

						EOS_EResult SetRawRichTextResult = EOS_PresenceModification_SetRawRichText(ChangeHandle, &RawRichTextOptions);
						if (SetRawRichTextResult != EOS_EResult::EOS_Success)
						{
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_SetRawRichText failed with result %s"), *LexToString(SetRawRichTextResult));
							InAsyncOp.SetError(Errors::FromEOSResult(SetRawRichTextResult));
							Promise.EmplaceValue();
							return;
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: RichText set to %s"), *Params.Mutations.StatusString.GetValue()); // Temp logging
						}
					}
					// Removed fields
					if (Params.Mutations.RemovedProperties.Num() > 0)
					{
						// EOS_PresenceModification_DeleteData
						TArray<FTCHARToUTF8, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> Utf8Strings;
						TArray<EOS_PresenceModification_DataRecordId, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> RecordIds;
						int32 CurrentIndex = 0;
						for (const FString& RemovedProperty : Params.Mutations.RemovedProperties)
						{
							const FTCHARToUTF8& Utf8Key = Utf8Strings.Emplace_GetRef(*RemovedProperty);

							EOS_PresenceModification_DataRecordId& RecordId = RecordIds.Emplace_GetRef();
							RecordId.ApiVersion = EOS_PRESENCEMODIFICATION_DATARECORDID_API_LATEST;
							RecordId.Key = Utf8Key.Get();

							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Removing field %s"), *RemovedProperty); // Temp logging
						}

						EOS_PresenceModification_DeleteDataOptions DataOptions = { };
						DataOptions.ApiVersion = EOS_PRESENCEMODIFICATION_DELETEDATA_API_LATEST;
						DataOptions.RecordsCount = RecordIds.Num();
						DataOptions.Records = RecordIds.GetData();
						EOS_EResult DeleteDataResult = EOS_PresenceModification_DeleteData(ChangeHandle, &DataOptions);
						if (DeleteDataResult != EOS_EResult::EOS_Success)
						{
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_DeleteDataOptions failed with result %s"), *LexToString(DeleteDataResult));
							InAsyncOp.SetError(Errors::FromEOSResult(DeleteDataResult));
							Promise.EmplaceValue();
							return;
						}
					}

					// Added/Updated fields
					if (Params.Mutations.UpdatedProperties.Num() > 0)
					{
						if (Params.Mutations.UpdatedProperties.Num() > EOS_PRESENCE_DATA_MAX_KEYS)
						{
							// TODO: Move this check higher.  Needs to take into account number of present fields (not just ones updated) and removed fields.
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Too many presence keys.  %u/%u"), Params.Mutations.UpdatedProperties.Num(), EOS_PRESENCE_DATA_MAX_KEYS);
							InAsyncOp.SetError(Errors::InvalidParams());
							Promise.EmplaceValue();
							return;
						}
						TArray<FTCHARToUTF8, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS * 2>> Utf8Strings;
						TArray<EOS_Presence_DataRecord, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> Records;
						int32 CurrentIndex = 0;
						for (const TPair<FString, FPresenceVariant>& UpdatedProperty : Params.Mutations.UpdatedProperties)
						{
							const FTCHARToUTF8& Utf8Key = Utf8Strings.Emplace_GetRef(*UpdatedProperty.Key);
							const FTCHARToUTF8& Utf8Value = Utf8Strings.Emplace_GetRef(*UpdatedProperty.Value); // TODO: Better serialization

							EOS_Presence_DataRecord& Record = Records.Emplace_GetRef();
							Record.ApiVersion = EOS_PRESENCE_DATARECORD_API_LATEST;
							Record.Key = Utf8Key.Get();
							Record.Value = Utf8Value.Get();
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: Set field [%s] to [%s]"), *UpdatedProperty.Key, *UpdatedProperty.Value); // Temp logging
						}

						EOS_PresenceModification_SetDataOptions DataOptions = { };
						DataOptions.ApiVersion = EOS_PRESENCEMODIFICATION_SETDATA_API_LATEST;
						DataOptions.RecordsCount = Records.Num();
						DataOptions.Records = Records.GetData();
						EOS_EResult SetDataResult = EOS_PresenceModification_SetData(ChangeHandle, &DataOptions);
						if (SetDataResult != EOS_EResult::EOS_Success)
						{
							UE_LOG(LogTemp, Warning, TEXT("UpdatePresence: EOS_PresenceModification_SetData failed with result %s"), *LexToString(SetDataResult));
							InAsyncOp.SetError(Errors::FromEOSResult(SetDataResult));
							Promise.EmplaceValue();
							return;
						}
					}

					EOS_Presence_SetPresenceOptions SetPresenceOptions = { };
					SetPresenceOptions.ApiVersion = EOS_PRESENCE_SETPRESENCE_API_LATEST;
					SetPresenceOptions.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
					SetPresenceOptions.PresenceModificationHandle = ChangeHandle;
					EOS_Async(EOS_Presence_SetPresence, PresenceHandle, SetPresenceOptions, MoveTemp(Promise));
					EOS_PresenceModification_Release(ChangeHandle);
					return;
				}
				else
				{
					InAsyncOp.SetError(Errors::FromEOSResult(CreatePresenceModificationResult));
				}
				Promise.EmplaceValue();
			})
			.Then([this](TOnlineAsyncOp<FPartialUpdatePresence>& InAsyncOp, const EOS_Presence_SetPresenceCallbackInfo* Data) mutable
			{
				UE_LOG(LogTemp, Warning, TEXT("SetPresenceResult: [%s]"), *LexToString(Data->ResultCode));

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					// Update local presence
					const FPartialUpdatePresence::Params& Params = InAsyncOp.GetParams();
					TSharedRef<FUserPresence> LocalUserPresence = FindOrCreatePresence(Params.LocalAccountId, Params.LocalAccountId);
					if (Params.Mutations.Status.IsSet())
					{
						LocalUserPresence->Status = Params.Mutations.Status.GetValue();
					}
					if (Params.Mutations.StatusString.IsSet())
					{
						LocalUserPresence->StatusString = Params.Mutations.StatusString.GetValue();
					}
					if (Params.Mutations.GameStatus.IsSet())
					{
						LocalUserPresence->GameStatus = Params.Mutations.GameStatus.GetValue();
					}
					if (Params.Mutations.Joinability.IsSet())
					{
						LocalUserPresence->Joinability = Params.Mutations.Joinability.GetValue();
					}
					for (const FString& RemovedKey : Params.Mutations.RemovedProperties)
					{
						LocalUserPresence->Properties.Remove(RemovedKey);
					}
					for (const TPair<FString, FPresenceVariant>& UpdatedProperty : Params.Mutations.UpdatedProperties)
					{
						LocalUserPresence->Properties.Emplace(UpdatedProperty.Key, UpdatedProperty.Value);
					}

					InAsyncOp.SetResult(FPartialUpdatePresence::Result());
				}
				else
				{
					InAsyncOp.SetError(Errors::FromEOSResult(Data->ResultCode));
				}
			})
			.Enqueue(GetSerialQueue());
	}
	return Op->GetHandle();
}

/** Get a user's presence, creating entries if missing */
TSharedRef<FUserPresence> FPresenceEOS::FindOrCreatePresence(FAccountId LocalAccountId, FAccountId PresenceAccountId)
{
	TMap<FAccountId, TSharedRef<FUserPresence>>& LocalUserPresenceList = PresenceLists.FindOrAdd(LocalAccountId);
	if (const TSharedRef<FUserPresence>* const ExistingPresence = LocalUserPresenceList.Find(PresenceAccountId))
	{
		return *ExistingPresence;
	}

	TSharedRef<FUserPresence> UserPresence = MakeShared<FUserPresence>();
	UserPresence->AccountId = PresenceAccountId;
	LocalUserPresenceList.Emplace(PresenceAccountId, UserPresence);
	return UserPresence;
}

void FPresenceEOS::UpdateUserPresence(FAccountId LocalAccountId, FAccountId PresenceAccountId)
{
	bool bPresenceHasChanged = false;
	TSharedRef<FUserPresence> UserPresence = FindOrCreatePresence(LocalAccountId, PresenceAccountId);
	// TODO:  Handle updates for local users.  Don't want to conflict with UpdatePresence calls

	// Get presence from EOS
	EOS_Presence_Info* PresenceInfo = nullptr;
	EOS_Presence_CopyPresenceOptions Options = { };
	Options.ApiVersion = EOS_PRESENCE_COPYPRESENCE_API_LATEST;
	Options.LocalUserId = GetEpicAccountIdChecked(LocalAccountId);
	Options.TargetUserId = GetEpicAccountIdChecked(PresenceAccountId);
	EOS_EResult CopyPresenceResult = EOS_Presence_CopyPresence(PresenceHandle, &Options, &PresenceInfo);
	if (CopyPresenceResult == EOS_EResult::EOS_Success)
	{
		// Convert the presence data to our format
		EUserPresenceStatus NewPresenceState = ToEPresenceState(PresenceInfo->Status);
		if (UserPresence->Status != NewPresenceState)
		{
			bPresenceHasChanged = true;
			UserPresence->Status = NewPresenceState;
		}

		FString NewStatusString = UTF8_TO_TCHAR(PresenceInfo->RichText);
		if (UserPresence->StatusString != NewStatusString)
		{
			bPresenceHasChanged = true;
			UserPresence->StatusString = MoveTemp(NewStatusString);
		}

		if (PresenceInfo->Records)
		{
			// TODO:  Handle Properties that aren't replicated through presence (eg "ProductId")
			TArrayView<const EOS_Presence_DataRecord> Records(PresenceInfo->Records, PresenceInfo->RecordsCount);
			// Detect removals
			TArray<FString, TInlineAllocator<EOS_PRESENCE_DATA_MAX_KEYS>> RemovedKeys;
			UserPresence->Properties.GenerateKeyArray(RemovedKeys);
			for (const EOS_Presence_DataRecord& Record : Records)
			{
				FString RecordKey = UTF8_TO_TCHAR(Record.Key);
				FString RecordValue = UTF8_TO_TCHAR(Record.Value);
				RemovedKeys.Remove(RecordKey);
				if (FString* ExistingValue = UserPresence->Properties.Find(RecordKey))
				{
					if (*ExistingValue == RecordValue)
					{
						continue; // No change
					}
				}
				bPresenceHasChanged = true;
				UserPresence->Properties.Add(MoveTemp(RecordKey), MoveTemp(RecordValue));
			}
			// Any fields that have been removed
			if (RemovedKeys.Num() > 0)
			{
				bPresenceHasChanged = true;
				for (const FString& RemovedKey : RemovedKeys)
				{
					UserPresence->Properties.Remove(RemovedKey);
				}
			}
		}
		else if (UserPresence->Properties.Num() > 0)
		{
			bPresenceHasChanged = true;
			UserPresence->Properties.Reset();
		}
		EOS_Presence_Info_Release(PresenceInfo);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateUserPresence: CopyPresence Failed %s"), *LexToString(CopyPresenceResult));
	}

	if (bPresenceHasChanged)
	{
		FPresenceUpdated PresenceUpdatedParams = { LocalAccountId, UserPresence };
		OnPresenceUpdatedEvent.Broadcast(PresenceUpdatedParams);
	}
}

/* UE::Online */ }
