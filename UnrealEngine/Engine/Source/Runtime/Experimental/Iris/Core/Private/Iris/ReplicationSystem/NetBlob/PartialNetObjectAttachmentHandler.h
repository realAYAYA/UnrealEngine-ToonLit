// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Iris/ReplicationSystem/NetBlob/SequentialPartialNetBlobHandler.h"
#include "Iris/ReplicationSystem/NetBlob/RawDataNetBlob.h"
#include "PartialNetObjectAttachmentHandler.generated.h"

UCLASS()
class UPartialNetObjectAttachmentHandlerConfig : public USequentialPartialNetBlobHandlerConfig
{
	GENERATED_BODY()

public:
	uint32 GetBitCountSplitThreshold() const { return BitCountSplitThreshold; }

private:
	/** How many bits a payload should have to recommend a split. Should be higher than MaxPartBitCount as splitting adds overhead. */
	UPROPERTY(Config)
	uint32 BitCountSplitThreshold = (128 + 64)*8;
};

struct FPartialNetObjectAttachmentHandlerInitParams
{
	UReplicationSystem* ReplicationSystem;
	const UPartialNetObjectAttachmentHandlerConfig* Config;
};

/**
 * NetBlobHandler that can split and assemble very large NetObjectAttachments.
 */
UCLASS(transient, MinimalAPI)
class UPartialNetObjectAttachmentHandler final : public USequentialPartialNetBlobHandler
{
	GENERATED_BODY()

public:
	UPartialNetObjectAttachmentHandler();
	virtual ~UPartialNetObjectAttachmentHandler();

	void Init(const FPartialNetObjectAttachmentHandlerInitParams& InitParams);

	/** Serializes the NetBlob and either store the serialized version in a new NetBlob or splits into multiple partial NetBlobs. */
	bool PreSerializeAndSplitNetBlob(uint32 ConnectionId, const TRefCountPtr<UE::Net::FNetObjectAttachment>& Blob, TArray<TRefCountPtr<FNetBlob>>& OutPartialBlobs, bool bSerializeWithObject) const;

	/** Splits a RawDataNetBlob. The blob must have been created by a registered NetBlobHandler in order to be reconstructed on the receiving side. */
	bool SplitRawDataNetBlob(const TRefCountPtr<UE::Net::FRawDataNetBlob>& Blob, TArray<TRefCountPtr<FNetBlob>>& OutPartialBlobs, const UE::Net::FNetDebugName* InDebugName) const;

	const UPartialNetObjectAttachmentHandlerConfig* GetConfig() const { return static_cast<const UPartialNetObjectAttachmentHandlerConfig*>(Super::GetConfig()); }
};
