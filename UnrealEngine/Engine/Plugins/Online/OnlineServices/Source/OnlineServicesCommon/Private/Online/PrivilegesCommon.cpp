// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/PrivilegesCommon.h"

#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

FPrivilegesCommon::FPrivilegesCommon(FOnlineServicesCommon& InServices)
	: TOnlineComponent(TEXT("Permission"), InServices)
{
}

void FPrivilegesCommon::RegisterCommands()
{
	RegisterCommand(&FPrivilegesCommon::QueryUserPrivilege);
}

TOnlineAsyncOpHandle<FQueryUserPrivilege> FPrivilegesCommon::QueryUserPrivilege(FQueryUserPrivilege::Params&& Params)
{
	TOnlineAsyncOpRef<FQueryUserPrivilege> Operation = GetOp<FQueryUserPrivilege>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

/* UE::Online */}