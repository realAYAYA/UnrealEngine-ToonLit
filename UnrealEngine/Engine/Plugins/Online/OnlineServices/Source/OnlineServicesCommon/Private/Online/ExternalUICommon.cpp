// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/ExternalUICommon.h"

#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

FExternalUICommon::FExternalUICommon(FOnlineServicesCommon& InServices)
	: TOnlineComponent(TEXT("ExternalUI"), InServices)
{
}

void FExternalUICommon::RegisterCommands()
{
	RegisterCommand(&FExternalUICommon::ShowFriendsUI);
}

TOnlineAsyncOpHandle<FExternalUIShowLoginUI> FExternalUICommon::ShowLoginUI(FExternalUIShowLoginUI::Params&& Params)
{
	TOnlineAsyncOpRef<FExternalUIShowLoginUI> Operation = GetOp<FExternalUIShowLoginUI>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FExternalUIShowFriendsUI> FExternalUICommon::ShowFriendsUI(FExternalUIShowFriendsUI::Params&& Params)
{
	TOnlineAsyncOpRef<FExternalUIShowFriendsUI> Operation = GetOp<FExternalUIShowFriendsUI>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineEvent<void(const FExternalUIStatusChanged&)> FExternalUICommon::OnExternalUIStatusChanged()
{
	return OnExternalUIStatusChangedEvent;
}

/* UE::Online */ }
