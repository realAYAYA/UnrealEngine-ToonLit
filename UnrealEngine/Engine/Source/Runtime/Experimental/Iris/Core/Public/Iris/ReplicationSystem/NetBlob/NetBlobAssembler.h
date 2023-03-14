// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Iris/ReplicationSystem/NetBlob/PartialNetBlob.h"
#include "Iris/Serialization/NetBitStreamWriter.h"

namespace UE::Net
{

/**
 * Utility class to reassemble split blobs.
 * Partial blobs need to be added in order and represent the same blob until it's assembled or an error occurs.
 */
class FNetBlobAssembler
{
public:
	IRISCORE_API FNetBlobAssembler();

	/**
	 * Add the next expected part of the split blob. If it's the first part of a blob the effects of prior calls to this function are discarded.
	 * @param Context FNetSerializationContext for error reporting. Call HasError() on it afterwards to check whether something went wrong.
	 * @param NetHandle The NetHandle needs to remain constant for every call until the original blob has been assembled or the first part of a new blob is added.
	 */
	IRISCORE_API void AddPartialNetBlob(FNetSerializationContext& Context, FNetHandle NetHandle, const TRefCountPtr<FPartialNetBlob>& PartialNetBlob);

	/** Returns true if all parts of the split blob have been added and is ready to be assembled. */
	bool IsReadyToAssemble() const { return PartCount > 0 && NextPartIndex == PartCount; }

	/**
	 * Assemble all parts of the split blob.
	 * @param Context FNetSerializationContext for error reporting. Call HasError() on it afterwards to check whether something went wrong.
	 * @return The assembled blob.
	 * @note IsReadyToAssemble() must return true before calling this function.
	 * @note After this function has been called it may not be called again until a new blob is ready to be assembled.
	 */
	IRISCORE_API TRefCountPtr<FNetBlob> Assemble(FNetSerializationContext& Context);

private:
	FNetBlobCreationInfo NetBlobCreationInfo;
	TArray<uint32> Payload;
	FNetBitStreamWriter BitWriter;
	FNetHandle NetHandle;
	uint32 NextPartIndex;
	uint32 PartCount;
	uint32 MaxByteCountPerPart;
};

}
