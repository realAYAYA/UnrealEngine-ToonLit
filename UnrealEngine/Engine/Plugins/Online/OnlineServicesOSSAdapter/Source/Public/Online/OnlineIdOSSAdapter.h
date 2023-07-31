// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/CoreOnline.h"
#include "OnlineSubsystemTypes.h"
#include "Misc/ScopeRWLock.h"

namespace UE::Online {

/**
 * A net id registry suitable for use with OSS FUniqueNetIds
 */
template<typename IdType>
class TOnlineUniqueNetIdRegistry : public IOnlineIdRegistry<IdType>
{
public:
	TOnlineUniqueNetIdRegistry(EOnlineServices InOnlineServicesType)
		: OnlineServicesType(InOnlineServicesType)
	{
	}

	virtual ~TOnlineUniqueNetIdRegistry() {}

	TOnlineId<IdType> FindOrAddHandle(const FUniqueNetIdRef& IdValue)
	{
		TOnlineId<IdType> OnlineId;
		if (!ensure(IdValue->IsValid()))
		{
			return OnlineId;
		}

		// Take a read lock and check if we already have a handle
		{
			FReadScopeLock ReadLock(Lock);
			if (const uint32* FoundHandle = IdValueToHandleMap.Find(IdValue))
			{
				OnlineId = TOnlineId<IdType>(OnlineServicesType, *FoundHandle);
			}
		}

		if (!OnlineId.IsValid())
		{
			// Take a write lock, check again if we already have a handle, or insert a new element.
			FWriteScopeLock WriteLock(Lock);
			if (const uint32* FoundHandle = IdValueToHandleMap.Find(IdValue))
			{
				OnlineId = TOnlineId<IdType>(OnlineServicesType, *FoundHandle);
			}

			if (!OnlineId.IsValid())
			{
				IdValues.Emplace(IdValue);
				OnlineId = TOnlineId<IdType>(OnlineServicesType, IdValues.Num());
				IdValueToHandleMap.Emplace(IdValue, OnlineId.GetHandle());
			}
		}

		return OnlineId;
	}

	// Returns a copy as it's not thread safe to return a pointer/ref to an element of an array that can be relocated by another thread.
	FUniqueNetIdPtr GetIdValue(const TOnlineId<IdType> OnlineId) const
	{
		if (OnlineId.GetOnlineServicesType() == OnlineServicesType && OnlineId.IsValid())
		{
			FReadScopeLock ReadLock(Lock);
			if (IdValues.IsValidIndex(OnlineId.GetHandle() - 1))
			{
				return IdValues[OnlineId.GetHandle() - 1];
			}
		}
		return FUniqueNetIdPtr();
	}

	FUniqueNetIdRef GetIdValueChecked(const TOnlineId<IdType> OnlineId) const
	{
		return GetIdValue(OnlineId).ToSharedRef();
	}

	bool IsHandleExpired(const FOnlineSessionId& InSessionId) const
	{
		return GetIdValue(InSessionId).IsValid();
	}

	// Begin IOnlineAccountIdRegistry
	virtual FString ToLogString(const TOnlineId<IdType>& OnlineId) const override
	{
		FUniqueNetIdPtr IdValue = GetIdValue(OnlineId);
		return IdValue ? IdValue->ToDebugString() : FString(TEXT("invalid_id"));
	}

	virtual TArray<uint8> ToReplicationData(const TOnlineId<IdType>& OnlineId) const override
	{
		return TArray<uint8>();
	}

	virtual TOnlineId<IdType> FromReplicationData(const TArray<uint8>& OnlineId) override
	{
		return TOnlineId<IdType>();
	}

	// End IOnlineAccountIdRegistry

private:
	mutable FRWLock Lock;

	EOnlineServices OnlineServicesType;
	TArray<FUniqueNetIdRef> IdValues;
	TUniqueNetIdMap<uint32> IdValueToHandleMap;
};

using FOnlineAccountIdRegistryOSSAdapter = TOnlineUniqueNetIdRegistry<OnlineIdHandleTags::FAccount>;

/* UE::Online */ }
