// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PlayerCore.h"
#include "HTTP/HTTPManager.h"
#include "Demuxer/ParserMKV.h"
#include "Utilities/Utilities.h"

namespace Electra
{

	class FMKVStaticDataReader : public IParserMKV::IReader
	{
	public:
		FMKVStaticDataReader() = default;
		virtual ~FMKVStaticDataReader() = default;
		virtual void SetParseData(TSharedPtrTS<IElectraHttpManager::FReceiveBuffer> InResponseBuffer)
		{
			ResponseBuffer = InResponseBuffer;
			DataSize = ResponseBuffer->Buffer.Num();
			Data = (const uint8*)ResponseBuffer->Buffer.GetLinearReadData();
			CurrentOffset = 0;
		}
	private:
		int64 MKVReadData(void* InDestinationBuffer, int64 InNumBytesToRead, int64 InFromOffset) override
		{
			if (InFromOffset >= DataSize)
			{
				return 0;
			}
			InNumBytesToRead = Utils::Min(DataSize - InFromOffset, InNumBytesToRead);
			if (InNumBytesToRead <= 0)
			{
				return 0;
			}
			CurrentOffset = InFromOffset;
			if (InDestinationBuffer)
			{
				FMemory::Memcpy(InDestinationBuffer, Data+CurrentOffset, InNumBytesToRead);
			}
			return InNumBytesToRead;
		}
		int64 MKVGetCurrentFileOffset() const override
		{ return CurrentOffset; }
		int64 MKVGetTotalSize() override
		{ return DataSize; }
		bool MKVHasReadBeenAborted() const override
		{ return false; }

		TSharedPtrTS<IElectraHttpManager::FReceiveBuffer> ResponseBuffer;
		const uint8* Data = nullptr;
		int64 DataSize = 0;
		int64 CurrentOffset = 0;
	};



} // namespace Electra
