// Copyright Epic Games, Inc. All Rights Reserved.
#include "Iris/ReplicationSystem/NetBlob/NetBlobAssembler.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlobHandler.h"
#include "Iris/ReplicationSystem/NetBlob/RawDataNetBlob.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetSerializationContext.h"

namespace UE::Net
{

static const FName NetError_PartialNetBlobSequenceError("Out of order PartialNetBlob.");

FNetBlobAssembler::FNetBlobAssembler()
: NetBlobCreationInfo({})
, NextPartIndex(0)
, PartCount(0)
, MaxByteCountPerPart(0)
{
}

void FNetBlobAssembler::AddPartialNetBlob(FNetSerializationContext& Context, FNetHandle InNetHandle, const TRefCountPtr<FPartialNetBlob>& PartialNetBlob)
{
	const uint32 PartIndex = PartialNetBlob->GetPartIndex();
	if (PartIndex != NextPartIndex)
	{
		Context.SetError(NetError_PartialNetBlobSequenceError);
		return;
	}

	++NextPartIndex;

	if (PartIndex == 0U)
	{
		PartCount = PartialNetBlob->GetPartCount();
		if (PartCount == 0)
		{
			Context.SetError(GNetError_InvalidValue);
			return;
		}

		NetHandle = InNetHandle;

		NetBlobCreationInfo = PartialNetBlob->GetOriginalCreationInfo();

		// The first part must be greater or equal to any subsequent parts or we will report an error.
		const uint32 PayloadBitCount = PartialNetBlob->GetPayloadBitCount();
		MaxByteCountPerPart = (PayloadBitCount + 7U)/8U;

		// Prepare a bitstream that can hold the full blob.
		Payload.SetNumUninitialized((MaxByteCountPerPart*PartCount + 3U)/4U);
		BitWriter.InitBytes(Payload.GetData(), Payload.Num()*4U);

		// Store partial payload.
		BitWriter.WriteBitStream(PartialNetBlob->GetPayload(), 0, PayloadBitCount);
	}
	else
	{
		if (InNetHandle != NetHandle)
		{
			Context.SetError(NetError_PartialNetBlobSequenceError);
			return;
		}

		const uint32 PayloadBitCount = PartialNetBlob->GetPayloadBitCount();
		const uint32 PayloadByteCount = (PayloadBitCount + 7U)/8U;
		if (PayloadByteCount > MaxByteCountPerPart)
		{
			Context.SetError(GNetError_InvalidValue);
			return;
		}

		// Store partial payload.
		BitWriter.WriteBitStream(PartialNetBlob->GetPayload(), 0, PayloadBitCount);
	}
}

TRefCountPtr<FNetBlob> FNetBlobAssembler::Assemble(FNetSerializationContext& Context)
{
	INetBlobReceiver* BlobHandler = Context.GetNetBlobReceiver();
	checkSlow(BlobHandler != nullptr);

	const TRefCountPtr<FNetBlob>& NetBlob = BlobHandler->CreateNetBlob(NetBlobCreationInfo);
	if (Context.HasError() || !NetBlob.IsValid())
	{
		Context.SetError(GNetError_UnsupportedNetBlob);
		return nullptr;
	}

	BitWriter.CommitWrites();

	// Fast path for RawDataNetBlobs
	if (EnumHasAnyFlags(NetBlob->GetCreationInfo().Flags, ENetBlobFlags::RawDataNetBlob))
	{
		FRawDataNetBlob* RawDataNetBlob = static_cast<FRawDataNetBlob*>(NetBlob.GetReference());
		// The BitWriter is currently pointing to the payload data so we want to make sure we're not leaving
		// it in an undefined state after moving the payload.
		const uint32 PayloadBitCount = BitWriter.GetPosBits();
		BitWriter = FNetBitStreamWriter();
		RawDataNetBlob->SetRawData(MoveTemp(Payload), PayloadBitCount);
	}
	else
	{
		FNetBitStreamReader BitReader;
		BitReader.InitBits(Payload.GetData(), BitWriter.GetPosBits());

		FNetSerializationContext ReadContext = Context.MakeSubContext(&BitReader);
		ReadContext.SetNetTraceCollector(nullptr);

		if (NetHandle.IsValid())
		{
			NetBlob->DeserializeWithObject(ReadContext, NetHandle);
		}
		else
		{
			NetBlob->Deserialize(ReadContext);
		}

		if (ReadContext.HasErrorOrOverflow())
		{
			// Copy error
			if (ReadContext.HasError())
			{
				Context.SetError(ReadContext.GetError());
			}
			else
			{
				Context.SetError(GNetError_BitStreamOverflow);
			}

			return nullptr;
		}

		// Bitstream mismatch?
		if (BitWriter.GetPosBits() != BitReader.GetPosBits())
		{
			Context.SetError(GNetError_BitStreamError);
			return nullptr;
		}
	}

	return NetBlob;
}

}
