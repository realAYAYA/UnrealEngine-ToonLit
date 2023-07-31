// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif


class FBitDataStream
{
public:
	FBitDataStream()
		: Data(nullptr), DataSize(0), BytePosition(0), BitPosition(0)
	{
	}

	FBitDataStream(const void* InData, uint64 InDataSize, uint64 StartBytePosition = 0, uint32 StartBitPosition = 0)
	{
		SetData(InData, InDataSize, StartBytePosition, StartBitPosition);
	}

	~FBitDataStream()
	{
	}

	void SetData(const void* InData, uint64 InDataSize, uint64 StartBytePosition = 0, uint32 StartBitPosition = 0)
	{
		check(InData && InDataSize);
		Data = (const uint8*)InData;
		DataSize = InDataSize;
		BytePosition = StartBytePosition;
		BitPosition = StartBitPosition;
	}

	bool IsByteAligned() const
	{
		return (BitPosition & 7) == 0;
	}

	const void* GetRemainingData() const
	{
		return Data + BytePosition;
	}

	uint64 GetRemainingByteLength() const
	{
		return DataSize - BytePosition;
	}

	uint64 GetRemainingBits() const
	{
		return GetRemainingByteLength() * 8 + (BitPosition ? 8 - BitPosition : 0);
	}

	void SkipBytes(uint64 nBytes)
	{
		BytePosition += nBytes;
	}

	void SkipBits(uint64 nBits)
	{
		SkipBytes(nBits >> 3);
		GetBits(nBits & 7);
	}

	uint32 GetBits(uint64 nBits)
	{
		uint32 rv = PeekBits(nBits);
		uint32 bp = BitPosition + nBits;
		BytePosition += bp >> 3;
		BitPosition = bp & 7;
		return rv;
	}

	uint32 PeekBits(uint64 nBits)
	{
		checkf(nBits <= 32, TEXT("This function can return at most 32 bits!"));
		if (nBits == 0)
		{
			return 0;
		}
		else
		{
			uint32 bytePos = BytePosition + (BitPosition >> 3);
			uint32 bitPos = BitPosition & 7;

			uint32 w;
			uint64 rl = GetRemainingByteLength();
			if (rl >= 4)
			{
#if defined(_MSC_VER)
				w = _byteswap_ulong(*(const uint32*)(Data + bytePos));
#else
				w = __builtin_bswap32(*(const uint32*)(Data + bytePos));
#endif
			}
			else
			{
				if (rl == 3)
				{
					w = ((uint32)Data[bytePos + 0] << 24) | ((uint32)Data[bytePos + 1] << 16) | ((uint32)Data[bytePos + 2] << 8);
				}
				else if (rl == 2)
				{
					w = ((uint32)Data[bytePos + 0] << 24) | ((uint32)Data[bytePos + 1] << 16);
				}
				else if (rl == 1)
				{
					w = ((uint32)Data[bytePos + 0] << 24);
				}
				else
				{
					w = 0;
				}

			}

			if (bitPos == 0)
			{
				w = w >> (32 - nBits);
				return w;
			}
			else
			{
				uint32 w0 = w;
				uint8 w1;
				if (rl > 4)
				{
					w1 = *(const uint8*)(Data + bytePos + 4);
				}
				else
				{
					w1 = 0;
				}
				w = (w0 << bitPos) | uint32(w1 >> (8 - bitPos));
				w = w >> (32 - nBits);
				return w;
			}
		}
	}

	uint64 GetBits64(uint64 nBits)
	{
		if (nBits <= 32)
		{
			return GetBits(nBits);
		}
		else
		{
			uint32 upper = GetBits(nBits - 32);
			uint32 lower = GetBits(32);
			return (uint64(upper) << 32ULL) | uint64(lower);
		}
	}

private:
	const uint8*	Data;
	uint64			DataSize;
	uint64			BytePosition;
	uint32			BitPosition;
};




namespace Electra
{

	struct FByteReader
	{
		FByteReader(const void* InData, int64 DataSize)
			: Data((const uint8*)InData)
			, NumBytesToGo(DataSize)
		{
		}

		bool ReadByte(uint8& To)
		{
			if (NumBytesToGo)
			{
				To = *Data++;
				--NumBytesToGo;
				return true;
			}
			return false;
		}

		bool ReadByte(uint16& To)
		{
			if (NumBytesToGo >= sizeof(uint16))
			{
				uint16 hi = *Data++;
				uint16 lo = *Data++;
				NumBytesToGo -= sizeof(uint16);
				To = (hi << 8) | lo;
				return true;
			}
			return false;
		}
		bool ReadBytes(uint8* To, int64 num)
		{
			if (NumBytesToGo >= num)
			{
				FMemory::Memcpy(To, Data, num);
				Data += num;
				NumBytesToGo -= num;
				return true;
			}
			return false;
		}
		int64 BytesRemaining() const
		{
			return NumBytesToGo;
		}

	private:
		const uint8* Data;
		int64			NumBytesToGo;
	};

} // namespace Electra


