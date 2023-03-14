// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Iris/ReplicationSystem/NetBlob/NetBlob.h"
#include "Iris/ReplicationSystem/NetBlob/RawDataNetBlob.h"
#include "Iris/Core/NetObjectReference.h"

namespace UE::Net
{

class FPartialNetBlob final : public FNetBlob
{
public:
	struct FSplitParams
	{
		uint32 MaxPartBitCount;
		uint32 MaxPartCount;
		FNetObjectReference NetObjectReference;
		FNetDebugName DebugName;
		bool bSerializeWithObject;
	};

	// Split a NetBlob into multiple PartialNetBlobs. The blob will be split even if the original one didn't need it.
	IRISCORE_API static bool SplitNetBlob(const FNetSerializationContext& Context, const FNetBlobCreationInfo& CreationInfo, const FSplitParams& SplitParams, const TRefCountPtr<FNetBlob>& Blob, TArray<TRefCountPtr<FNetBlob>>& OutPartialBlobs);

	// Split a RawDataNetBlob into multiple PartialNetBlobs. The blob will be split even if the original one didn't need it.
	IRISCORE_API static bool SplitNetBlob(const FNetBlobCreationInfo& CreationInfo, const FSplitParams& SplitParams, const TRefCountPtr<FRawDataNetBlob>& Blob, TArray<TRefCountPtr<FNetBlob>>& OutPartialBlobs);

public:
	IRISCORE_API FPartialNetBlob(const FNetBlobCreationInfo& CreationInfo);

	uint32 GetPartIndex() const { return PartIndex; }

	// This will only return legible data for the first part on the receiving end.
	uint32 GetPartCount() const { return (PartIndex == 0U ? PartCount : 0U); }

	uint32 GetPayloadBitCount() const { return PayloadBitCount; }
	const uint32* GetPayload() const { return Payload.GetData(); }

	const FNetBlobCreationInfo& GetOriginalCreationInfo() const { return OriginalCreationInfo; }

	void SetDebugName(const FNetDebugName& InDebugName) { DebugName = InDebugName; }

private:

	bool IsFirstPart() const { return PartIndex == 0; }
	bool IsLastPart() const { return PartIndex+1 == PartCount; }

	virtual TArrayView<const FNetObjectReference> GetExports() const override final;
	virtual void SerializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle) const override;
	virtual void DeserializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle) override;

	virtual void Serialize(FNetSerializationContext& Context) const override;
	virtual void Deserialize(FNetSerializationContext& Context) override;

	void InternalSerialize(FNetSerializationContext& Context) const;
	void InternalDeserialize(FNetSerializationContext& Context);

	void InternalSerializeBlob(FNetSerializationContext& Context) const;
	void InternalDeserializeBlob(FNetSerializationContext& Context);

	struct FPayloadSplitParams
	{
		FNetDebugName DebugName;

		FNetBlobCreationInfo CreationInfo;
		FNetBlobCreationInfo OriginalCreationInfo;
		FNetBlob* OriginalBlob;
		const uint32* Payload;
		uint32 PayloadBitCount;
		uint32 PartBitCount;
	};
	static void SplitPayload(const FPayloadSplitParams& SplitParams, TArray<TRefCountPtr<FNetBlob>>& OutPartialBlobs);

	// Used by the first part to be able to reconstruct the original message.
	FNetBlobCreationInfo OriginalCreationInfo;
	TRefCountPtr<FNetBlob> OriginalBlob;

	uint16 PartIndex;
	// PartCount is only valid if it's the first part.
	uint16 PartCount;
	uint16 PayloadBitCount;
	// Use uint32 for guaranteed FNetBitStreamReader/Writer compatibility
	TArray<uint32> Payload;

	FNetDebugName DebugName;
};

}
