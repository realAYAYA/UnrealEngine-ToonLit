// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PlayerCore.h"
#include "HTTP/HTTPManager.h"
#include "Demuxer/ParserISO14496-12.h"

namespace Electra
{

	class FMP4StaticDataReader : public IParserISO14496_12::IReader
	{
	public:
		FMP4StaticDataReader() = default;
		virtual ~FMP4StaticDataReader() = default;
		virtual void SetParseData(TSharedPtrTS<IElectraHttpManager::FReceiveBuffer> InResponseBuffer)
		{
			ResponseBuffer = InResponseBuffer;
			DataSize = ResponseBuffer->Buffer.Num();
			Data = (const uint8*)ResponseBuffer->Buffer.GetLinearReadData();
			CurrentOffset = 0;
		}
	private:
		//----------------------------------------------------------------------
		// Methods from IParserISO14496_12::IReader
		//
		virtual int64 ReadData(void* IntoBuffer, int64 NumBytesToRead) override
		{
			if (NumBytesToRead <= DataSize - CurrentOffset)
			{
				if (IntoBuffer)
				{
					FMemory::Memcpy(IntoBuffer, Data+CurrentOffset, NumBytesToRead);
				}
				CurrentOffset += NumBytesToRead;
				return NumBytesToRead;
			}
			return -1;
		}
		virtual bool HasReachedEOF() const override
		{
			return ResponseBuffer->Buffer.GetEOD() && CurrentOffset >= DataSize;
		}
		virtual bool HasReadBeenAborted() const override
		{
			return ResponseBuffer->Buffer.WasAborted();
		}
		virtual int64 GetCurrentOffset() const override
		{
			return CurrentOffset;
		}

		TSharedPtrTS<IElectraHttpManager::FReceiveBuffer> ResponseBuffer;
		const uint8* Data = nullptr;
		int64 DataSize = 0;
		int64 CurrentOffset = 0;
	};



} // namespace Electra
