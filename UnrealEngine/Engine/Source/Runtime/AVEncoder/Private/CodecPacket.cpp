// Copyright Epic Games, Inc. All Rights Reserved.

#include "CodecPacket.h"
#include "CoreMinimal.h"

namespace
{
	struct FDataCopyDeleter
	{
		void operator()(uint8* p)
		{
			FMemory::Free(p);
		}
	};

	void DataCopyDeleter(uint8* p)
	{
		FMemory::Free(p);
	}
}

namespace AVEncoder
{
	FCodecPacket FCodecPacket::Create(const uint8* InData, uint32 InDataSize)
	{
		uint8* DataCopy = static_cast<uint8*>(FMemory::Malloc(InDataSize));
		FMemory::BigBlockMemcpy(DataCopy, InData, InDataSize);

		TSharedPtr<uint8> Data(DataCopy, [](uint8* Obj){ DataCopyDeleter(Obj); });

		FCodecPacket Packet;
		Packet.Data = Data;
		Packet.DataSize = InDataSize;
		return Packet;
	}
} /* namespace AVEncoder */
