// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PUSH_MODEL

#include "CoreTypes.h"
#include "Iris/ReplicationSystem/NetHandle.h"
#include "Net/Core/PushModel/PushModel.h"

namespace UE::Net
{
	struct FReplicationProtocol;
	struct FReplicationInstanceProtocol;
}

namespace UE::Net::Private
{

extern IRISCORE_API bool bIsIrisPushModelForceEnabled;
extern IRISCORE_API int IrisPushModelMode;
inline bool IsIrisPushModelEnabled(bool bIsPushModelEnabled = IS_PUSH_MODEL_ENABLED()) { return (bIsIrisPushModelForceEnabled | (IrisPushModelMode > 0)) & bIsPushModelEnabled; }

struct FNetPushObjectHandle
{
public:
	FNetPushObjectHandle() : Value(Invalid) {}
	FNetPushObjectHandle(uint32 InIndex, uint32 InReplicationSystemId) : Index(InIndex), ReplicationSystemId(InReplicationSystemId + 1U), ZeroPadding(0) {}

	uint32 GetInternalIndex() const { return Index; }
	uint32 GetReplicationSystemId() const { check(ReplicationSystemId != 0U); return ReplicationSystemId - 1U; }
	bool IsValid() const { return Value != Invalid; }

private:
	friend struct FNetHandleLegacyPushModelHelper;

	enum : uint64 { Invalid = 0 };

	explicit FNetPushObjectHandle(UEPushModelPrivate::FNetIrisPushObjectId PushId) : Value(PushId) {}
	uint64 GetValue() const { return Value; }

	union 
	{
		struct
		{
			uint64 Index : FNetHandle::IdBits;								// Ever increasing index, we use different indices for static and dynamic object
			uint64 ReplicationSystemId : FNetHandle::ReplicationSystemIdBits;	// ReplicationSystemId, when running in pie, we track the owning instance
			uint64 ZeroPadding : (64 - FNetHandle::IdBits - FNetHandle::ReplicationSystemIdBits);
		};
		uint64 Value : 64;
	};
};

struct FNetHandleLegacyPushModelHelper
{
	static void InitPushModel();
	static void ShutdownPushModel();

	IRISCORE_API static void SetNetPushID(UObject* Object, FNetPushObjectHandle PushHandle);
	IRISCORE_API static void ClearNetPushID(UObject* Object);

private:
	static void MarkPropertyOwnerDirty(const UObject* Object, UEPushModelPrivate::FNetIrisPushObjectId PushId, const int32 RepIndex);
	static void MarkPropertiesOwnerDirty(const UObject* Object, UEPushModelPrivate::FNetIrisPushObjectId PushId, const int32 StartRepIndex, const int32 EndRepIndex);

	static void MarkPropertyDirty(const UObject* Object, UEPushModelPrivate::FNetIrisPushObjectId PushId, const int32 RepIndex);
	static void MarkPropertiesDirty(const UObject* Object, UEPushModelPrivate::FNetIrisPushObjectId PushId, const int32 StartRepIndex, const int32 EndRepIndex);

	static void OptionallyMarkPropertyOwnerDirty(const UObject* Object, UEPushModelPrivate::FNetIrisPushObjectId PushId, const int32 RepIndex);
	static void OptionallyMarkPropertiesOwnerDirty(const UObject* Object, UEPushModelPrivate::FNetIrisPushObjectId PushId, const int32 StartRepIndex, const int32 EndRepIndex);
};

}

#define UE_NET_IRIS_SET_PUSH_ID(...) UE::Net::Private::FNetHandleLegacyPushModelHelper::SetNetPushID(__VA_ARGS__)
#define UE_NET_IRIS_CLEAR_PUSH_ID(...) UE::Net::Private::FNetHandleLegacyPushModelHelper::ClearNetPushID(__VA_ARGS__)

#define UE_NET_IRIS_INIT_LEGACY_PUSH_MODEL() UE::Net::Private::FNetHandleLegacyPushModelHelper::InitPushModel()
#define UE_NET_IRIS_SHUTDOWN_LEGACY_PUSH_MODEL()  UE::Net::Private::FNetHandleLegacyPushModelHelper::ShutdownPushModel()

#else

namespace UE::Net::Private
{

inline constexpr bool IsIrisPushModelEnabled(bool bIsPushModelEnabled = false) { return false; }

}

#define UE_NET_IRIS_SET_PUSH_ID(...)
#define UE_NET_IRIS_CLEAR_PUSH_ID(...)

#define UE_NET_IRIS_INIT_LEGACY_PUSH_MODEL() 
#define UE_NET_IRIS_SHUTDOWN_LEGACY_PUSH_MODEL() 

#endif
