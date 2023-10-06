// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Privileges.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMON_API FPrivilegesCommon : public TOnlineComponent<IPrivileges>
{
public:
	using Super = IPrivileges;

	FPrivilegesCommon(FOnlineServicesCommon& InServices);

	// TOnlineComponent
	virtual void RegisterCommands() override;

	// IPermission
	virtual TOnlineAsyncOpHandle<FQueryUserPrivilege> QueryUserPrivilege(FQueryUserPrivilege::Params&& Params) override;
};

/* UE::Online */}