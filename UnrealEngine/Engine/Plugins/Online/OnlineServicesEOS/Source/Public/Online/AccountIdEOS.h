// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/CoreOnline.h"

typedef struct EOS_EpicAccountIdDetails* EOS_EpicAccountId;
typedef struct EOS_ProductUserIdDetails* EOS_ProductUserId;

namespace UE::Online
{

ONLINESERVICESEOS_API EOS_EpicAccountId GetEpicAccountId(const FAccountId& AccountId);
ONLINESERVICESEOS_API EOS_EpicAccountId GetEpicAccountIdChecked(const FAccountId& AccountId);
ONLINESERVICESEOS_API FAccountId FindAccountId(const EOS_EpicAccountId EpicAccountId);
ONLINESERVICESEOS_API FAccountId FindAccountIdChecked(const EOS_EpicAccountId EpicAccountId);
ONLINESERVICESEOS_API FAccountId CreateAccountId(const EOS_EpicAccountId EpicAccountId, const EOS_ProductUserId ProductUserId);

} /* namespace UE::Online */
