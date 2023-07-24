// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/ExternalUICommon.h"

class IOnlineExternalUI;
using IOnlineExternalUIPtr = TSharedPtr<IOnlineExternalUI>;

namespace UE::Online {

class FExternalUIOSSAdapter : public FExternalUICommon
{
public:
	using Super = FExternalUICommon;

	using FExternalUICommon::FExternalUICommon;

	// TOnlineComponent
	virtual void PostInitialize() override;
	virtual void PreShutdown() override;

	// IExternalUI
	virtual TOnlineAsyncOpHandle<FExternalUIShowLoginUI> ShowLoginUI(FExternalUIShowLoginUI::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FExternalUIShowFriendsUI> ShowFriendsUI(FExternalUIShowFriendsUI::Params&& Params) override;

	IOnlineExternalUIPtr GetExternalUIInterface() const;

protected:
	FDelegateHandle OnExternalUIChangeHandle;
};

/* UE::Online */ }
