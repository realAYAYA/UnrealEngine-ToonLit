// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/CoreOnline.h"
#include "Online/OnlineIdCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"

namespace UE::Online {

class IOnlineAccountIdRegistryEOSGS : public IOnlineAccountIdRegistry
{
public:
	virtual FAccountId FindAccountId(EOS_ProductUserId ProductUserId) const = 0;
	virtual EOS_ProductUserId GetProductUserId(const FAccountId& AccountId) const = 0;
};

/**
 * Account id registry specifically for EOS id's which are segmented.
 */
class FOnlineAccountIdRegistryEOSGS
	: public IOnlineAccountIdRegistryEOSGS
{
public:
	FOnlineAccountIdRegistryEOSGS();
	virtual ~FOnlineAccountIdRegistryEOSGS() = default;

	// Begin IOnlineAccountIdRegistryEOSGS
	virtual FAccountId FindAccountId(const EOS_ProductUserId ProductUserId) const override;
	virtual EOS_ProductUserId GetProductUserId(const FAccountId& AccountId) const override;
	// End IOnlineAccountIdRegistryEOSGS

	// Begin IOnlineAccountIdRegistry
	virtual FString ToLogString(const FAccountId& AccountId) const override;
	virtual TArray<uint8> ToReplicationData(const FAccountId& AccountId) const override;
	virtual FAccountId FromReplicationData(const TArray<uint8>& ReplicationData) override;
	// End IOnlineAccountIdRegistry

	static IOnlineAccountIdRegistryEOSGS& GetRegistered();

private:
	// FAuthEOSGS is the only thing that should be able to create PUID-only net ids in this registry, in its resolve methods.
	friend class FAuthEOSGS;
	friend class FOnlineServicesEOSGSModule;
	static FOnlineAccountIdRegistryEOSGS& Get();
	FAccountId FindOrAddAccountId(const EOS_ProductUserId ProductUserId);

	TOnlineBasicAccountIdRegistry<EOS_ProductUserId> Registry;
};

EOS_ProductUserId ONLINESERVICESEOSGS_API GetProductUserId(const FAccountId& AccountId);
EOS_ProductUserId ONLINESERVICESEOSGS_API GetProductUserIdChecked(const FAccountId& AccountId);
FAccountId ONLINESERVICESEOSGS_API FindAccountId(const EOS_ProductUserId EpicAccountId);
FAccountId ONLINESERVICESEOSGS_API FindAccountIdChecked(const EOS_ProductUserId EpicAccountId);

template<typename IdType>
inline bool ValidateOnlineId(const TOnlineId<IdType> OnlineId)
{
	return OnlineId.GetOnlineServicesType() == EOnlineServices::Epic && OnlineId.IsValid();
}

} /* namespace UE::Online */
