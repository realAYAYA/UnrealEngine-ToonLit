// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Cooker/CompactBinaryTCP.h"
#include "Cooker/CookTypes.h"
#include "HAL/Platform.h"
#include "Misc/Guid.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"

class FCbObject;
class FCbObjectView;
class FCbWriter;
class ITargetPlatform;

namespace UE::Cook { class FCookWorkerClient; }
namespace UE::Cook { class FCookWorkerServer; }

namespace UE::Cook
{

class FMPCollectorClientTickContext
{
public:
	TConstArrayView<const ITargetPlatform*> GetPlatforms() const { return Platforms; }
	bool IsFlush()  const { return bFlush; }

	void AddMessage(FCbObject Object);

	uint8 PlatformToInt(const ITargetPlatform* Platform) const;
	const ITargetPlatform* IntToPlatform(uint8 PlatformAsInt) const;

private:
	TConstArrayView<const ITargetPlatform*> Platforms;
	TArray<FCbObject> Messages;
	bool bFlush = false;

	friend class FCookWorkerClient;
};

class FMPCollectorClientTickPackageContext
{
public:
	struct FPlatformData
	{
		const ITargetPlatform* TargetPlatform = nullptr;
		ECookResult CookResults = ECookResult::NotAttempted;
	};
	TConstArrayView<const ITargetPlatform*> GetPlatforms() const { return Platforms; }
	TConstArrayView<FPlatformData> GetPlatformDatas() const { return PlatformDatas; }
	FName GetPackageName() const { return PackageName; }

	void AddMessage(FCbObject Object);
	void AddAsyncMessage(TFuture<FCbObject>&& ObjectFuture);
	void AddPlatformMessage(const ITargetPlatform* Platform, FCbObject Object);
	void AddAsyncPlatformMessage(const ITargetPlatform* Platform, TFuture<FCbObject>&& ObjectFuture);

	uint8 PlatformToInt(const ITargetPlatform* Platform) const;
	const ITargetPlatform* IntToPlatform(uint8 PlatformAsInt) const;

private:
	TArray<TPair<const ITargetPlatform*, FCbObject>> Messages;
	TArray<TPair<const ITargetPlatform*, TFuture<FCbObject>>> AsyncMessages;
	TConstArrayView<const ITargetPlatform*> Platforms;
	TConstArrayView<FPlatformData> PlatformDatas;
	FName PackageName;

	friend class FCookWorkerClient;
};

class FMPCollectorClientMessageContext
{
public:
	TConstArrayView<const ITargetPlatform*> GetPlatforms() { return Platforms; }

	uint8 PlatformToInt(const ITargetPlatform* Platform) const;
	const ITargetPlatform* IntToPlatform(uint8 PlatformAsInt) const;

private:
	TConstArrayView<const ITargetPlatform*> Platforms;

	friend class FCookWorkerClient;
};

class FMPCollectorServerMessageContext
{
public:
	TConstArrayView<const ITargetPlatform*> GetPlatforms() { return Platforms; }

	uint8 PlatformToInt(const ITargetPlatform* Platform) const;
	const ITargetPlatform* IntToPlatform(uint8 PlatformAsInt) const;
	FWorkerId GetWorkerId() const { return WorkerId; }
	int32 GetProfileId() const { return ProfileId; }
	FCookWorkerServer* GetCookWorkerServer() { return Server; }

	bool HasPackageName() const { return !PackageName.IsNone(); }
	FName GetPackageName() const { return PackageName; }
	bool HasTargetPlatform() const { return TargetPlatform != nullptr; }
	const ITargetPlatform* GetTargetPlatform() const { return TargetPlatform; }

private:
	TConstArrayView<const ITargetPlatform*> Platforms;
	FName PackageName;
	FCookWorkerServer* Server = nullptr;
	const ITargetPlatform* TargetPlatform = nullptr;
	int32 ProfileId;
	FWorkerId WorkerId;

	friend class FCookWorkerServer;
};

/**
 * Interface used during cooking to send data collected from save/load on a remote CookWorker
 * to the Director for aggregation into files saves at the end of the cook. 
 */
class IMPCollector : public FRefCountBase
{
public:
	virtual ~IMPCollector() {}

	virtual FGuid GetMessageType() const = 0;
	virtual const TCHAR* GetDebugName() const = 0;

	virtual void ClientTick(FMPCollectorClientTickContext& Context) {}
	virtual void ClientTickPackage(FMPCollectorClientTickPackageContext& Context) {}
	virtual void ClientReceiveMessage(FMPCollectorClientMessageContext& Context, FCbObjectView Message) {}
	virtual void ServerReceiveMessage(FMPCollectorServerMessageContext& Context, FCbObjectView Message) {}
};

/** A subinterface of IMPCollector that uses a UE::CompactBinaryTCP::IMessage subclass to serialize the message. */
template <typename CompactBinaryTCPMessageType>
class IMPCollectorCbMessage : public IMPCollector
{
public:
	virtual void ClientReceiveMessage(FMPCollectorClientMessageContext& Context, bool bReadSuccessful,
		CompactBinaryTCPMessageType&& Message) {}
	virtual void ServerReceiveMessage(FMPCollectorServerMessageContext& Context, bool bReadSuccessful,
		CompactBinaryTCPMessageType&& Message) {}

	virtual FGuid GetMessageType() const override
	{
		return CompactBinaryTCPMessageType::MessageType;
	}

	virtual void ClientReceiveMessage(FMPCollectorClientMessageContext& Context, FCbObjectView Message) override
	{
		CompactBinaryTCPMessageType CBMessage;
		bool bReadSuccessful = CBMessage.TryRead(Message);
		ClientReceiveMessage(Context, bReadSuccessful, MoveTemp(CBMessage));
	}

	virtual void ServerReceiveMessage(FMPCollectorServerMessageContext& Context, FCbObjectView Message) override
	{
		CompactBinaryTCPMessageType CBMessage;
		bool bReadSuccessful = CBMessage.TryRead(Message);
		ServerReceiveMessage(Context, bReadSuccessful, MoveTemp(CBMessage));
	}
};

/**
 * An implementation of IMPCollector that uses a UE::CompactBinaryTCP::IMessage subclass to serialize the message,
 * and directs messages received on the client to the given callback.
 */
template <typename CompactBinaryTCPMessageType>
class IMPCollectorCbClientMessage : public IMPCollectorCbMessage<CompactBinaryTCPMessageType>
{
public:
	IMPCollectorCbClientMessage(TUniqueFunction<void(FMPCollectorClientMessageContext& Context, bool bReadSuccessful,
		CompactBinaryTCPMessageType&& Message)>&& InCallback, const TCHAR* InDebugName)
		: Callback(MoveTemp(InCallback))
		, DebugName(InDebugName)
	{
		check(Callback);
	}

	virtual const TCHAR* GetDebugName() const override { return DebugName; }

	virtual void ClientReceiveMessage(FMPCollectorClientMessageContext& Context, bool bReadSuccessful,
		CompactBinaryTCPMessageType&& Message) override
	{
		Callback(Context, bReadSuccessful, MoveTemp(Message));
	}

private:
	TUniqueFunction<void(FMPCollectorClientMessageContext& Context, bool bReadSuccessful, CompactBinaryTCPMessageType&& Message)> Callback;
	const TCHAR* DebugName;
};

/**
 * An implementation of IMPCollector that uses a UE::CompactBinaryTCP::IMessage subclass to serialize the message,
 * and directs messages received on the server to the given callback.
 */
template <typename CompactBinaryTCPMessageType>
class IMPCollectorCbServerMessage : public IMPCollectorCbMessage<CompactBinaryTCPMessageType>
{
public:
	IMPCollectorCbServerMessage(TUniqueFunction<void(FMPCollectorServerMessageContext& Context, bool bReadSuccessful,
		CompactBinaryTCPMessageType&& Message)>&& InCallback, const TCHAR* InDebugName)
		: Callback(MoveTemp(InCallback))
		, DebugName(InDebugName)
	{
		check(Callback);
	}

	virtual const TCHAR* GetDebugName() const override { return DebugName; }

	virtual void ServerReceiveMessage(FMPCollectorServerMessageContext& Context, bool bReadSuccessful,
		CompactBinaryTCPMessageType&& Message) override
	{
		Callback(Context, bReadSuccessful, MoveTemp(Message));
	}

private:
	TUniqueFunction<void(FMPCollectorServerMessageContext& Context, bool bReadSuccessful, CompactBinaryTCPMessageType&& Message)> Callback;
	const TCHAR* DebugName;
};

}