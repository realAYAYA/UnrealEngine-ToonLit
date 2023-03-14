// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineError.h"

// Workaround for not being able to clear a delegate handle for a lambda while the lambda is being executed
void OnOnlineSessionStartMatchmakingBroadcast(FName DelegateSessionName, bool bWasSuccessful, IOnlineSession* OnlineSession, FName RequestedSessionName, FDelegateHandle* DelegateHandle, const FOnStartMatchmakingComplete CompletionDelegate)
{
	if (DelegateSessionName == RequestedSessionName)
	{
		OnlineSession->ClearOnMatchmakingCompleteDelegate_Handle(*DelegateHandle);
		delete DelegateHandle;

		FOnlineError OnlineError(bWasSuccessful);
		CompletionDelegate.ExecuteIfBound(RequestedSessionName, OnlineError, FSessionMatchmakingResults());
	}
}

bool IOnlineSession::StartMatchmaking(const TArray<FSessionMatchmakingUser>& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings, const FOnStartMatchmakingComplete& CompletionDelegate)
{
	// TODO: Deprecate other StartMatchmaking function in favor of this when this is implemented on all platforms
	TArray<FUniqueNetIdRef> LocalPlayerIds;
	LocalPlayerIds.Reserve(LocalPlayers.Num());
	for (const FSessionMatchmakingUser& LocalPlayer : LocalPlayers)
	{
		LocalPlayerIds.Emplace(LocalPlayer.UserId);
	}
	bool bStartMatchmakingSuccess = StartMatchmaking(LocalPlayerIds, SessionName, NewSessionSettings, SearchSettings);
	if (bStartMatchmakingSuccess)
	{
		FDelegateHandle* DelegateHandle = new FDelegateHandle;
		*DelegateHandle = AddOnMatchmakingCompleteDelegate_Handle(FOnMatchmakingCompleteDelegate::CreateStatic(OnOnlineSessionStartMatchmakingBroadcast, this, SessionName, DelegateHandle, CompletionDelegate));
	}
	return bStartMatchmakingSuccess;
}

void IOnlineSession::RemovePlayerFromSession(int32 LocalUserNum, FName SessionName, const FUniqueNetId& TargetPlayerId)
{
	UE_LOG_ONLINE_SESSION(Warning, TEXT("[IOnlineSession::RemovePlayerFromSession] This functionality is not implemented by default."));
}