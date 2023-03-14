// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CompactBinaryTCP.h"
#include "Containers/Array.h"
#include "CookTypes.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

class ITargetPlatform;
namespace UE::Cook { struct FPackageData; }

namespace UE::Cook
{

/**
 * The base class of messages that can be sent as platform-specific package-specific submessages
 * in a FPackageResultMessage. Messages are identified to the remote connection by the Guid identifier
 * from GetMessageType.
 */
class IPackageMessage
{
public:
	virtual ~IPackageMessage() {}
	/** Marshall the message to a CompactBinaryObject. */
	virtual void Write(FCbWriter& Writer, const FPackageData& PackageData, const ITargetPlatform* TargetPlatform) const = 0;
	/** Unmarshall the message from a CompactBinaryObject. */
	virtual bool TryRead(FCbObject&& Object, FPackageData& PackageData, const ITargetPlatform* TargetPlatform) = 0;
	/** Return the Guid that identifies the message to the remote connection. */
	virtual FGuid GetMessageType() const = 0;
};

/**
 * Helper struct for FPackageResultsMessage that can also be owned by other types like PackageData.
 * Holds replication information about the result of the Package's save on each platform, including
 * system-specific messages from other systems
 */
struct FPackageRemoteResult
{
public:
	/** Information about the results for a single platform */
	struct FPlatformResult
	{
		const ITargetPlatform* Platform = nullptr;
		TArray<UE::CompactBinaryTCP::FMarshalledMessage> Messages;
		FGuid PackageGuid;
		FCbObject TargetDomainDependencies;
		bool bSuccessful = true;
	};

	void AddMessage(const FPackageData& PackageData, const ITargetPlatform* TargetPlatform, const IPackageMessage& Message);

public:
	FName PackageName;
	/** If failure reason is InvalidSuppressCookReason, it was saved. Otherwise, holds the suppression reason */
	ESuppressCookReason SuppressCookReason;
	bool bReferencedOnlyByEditorOnlyData = false;
	TArray<FPlatformResult, TInlineAllocator<1>> Platforms;

};

/** Message from Client to Server giving the results for saved or refused-to-cook packages. */
struct FPackageResultsMessage : public UE::CompactBinaryTCP::IMessage
{
public:
	virtual void Write(FCbWriter& Writer) const override;
	virtual bool TryRead(FCbObject&& Object) override;
	virtual FGuid GetMessageType() const override { return MessageType; }

public:
	TArray<FPackageRemoteResult> Results;

	static FGuid MessageType;
};

}
