// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtils.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AssertionMacros.h"
#include <stdint.h>

namespace UE::Net
{

FNetBitStreamReader::FNetBitStreamReader()
: Buffer(nullptr)
, BufferBitCapacity(0)
, BufferBitPosition(0)
, PendingWord(0)
, OverflowBitCount(0)
{
}

FNetBitStreamReader::~FNetBitStreamReader()
{
}

void FNetBitStreamReader::InitBits(const void* InBuffer, uint32 BitCount)
{
	check(InBuffer != nullptr);
	checkf((uintptr_t(InBuffer) & 3) == 0, TEXT("Buffer needs to be 4-byte aligned."));
	Buffer = static_cast<const uint32*>(InBuffer);
	BufferBitCapacity = BitCount;
	BufferBitPosition = 0;
	if (BitCount > 0)
	{
		PendingWord = INTEL_ORDER32(Buffer[0]);
	}
}

uint32 FNetBitStreamReader::ReadBits(uint32 BitCount)
{
	if (OverflowBitCount != 0)
	{
		return 0U;
	}

	if (BufferBitCapacity - BufferBitPosition < BitCount)
	{
		OverflowBitCount = BitCount - (BufferBitCapacity - BufferBitPosition);
		return 0U;
	}

	const uint32 CurrentBufferBitPosition = BufferBitPosition;
	const uint32 BitCountUsedInWord = BufferBitPosition & 31;
	const uint32 BitCountLeftInWord = 32 - (BufferBitPosition & 31);

	BufferBitPosition += BitCount;

	// If after the read we still have unused bits in the PendingWord we can skip loading a new word.
	if (BitCountLeftInWord > BitCount)
	{
		const uint32 PendingWordMask = ((1U << BitCount) - 1U);
		const uint32 Value = (PendingWord >> BitCountUsedInWord) & PendingWordMask;
		return Value;
	}
	else
	{
		uint32 Value = PendingWord >> BitCountUsedInWord;
		if ((BufferBitPosition & ~31U) < BufferBitCapacity)
		{
			// BitCountToRead will be in range [0, 31] as we've already written at least one bit at this point
			const uint32 BitCountToRead = BitCount - BitCountLeftInWord;
			const uint32 Word = INTEL_ORDER32(Buffer[BufferBitPosition >> 5U]);
			const uint32 WordMask = (1U << BitCountToRead) - 1U;

			Value = ((Word & WordMask) << (BitCountLeftInWord & 31)) | Value;
			PendingWord = Word;
		}

		return Value;
	}
}

void FNetBitStreamReader::ReadBitStream(uint32* InDst, uint32 BitCount)
{
	if (OverflowBitCount != 0)
	{
		return;
	}

	if (BufferBitCapacity - BufferBitPosition < BitCount)
	{
		OverflowBitCount = BitCount - (BufferBitCapacity - BufferBitPosition);
		return;
	}

	uint32 CurSrcBit = BufferBitPosition;
	const uint32* RESTRICT Src = Buffer;
	uint32* RESTRICT Dst = InDst;
	uint32 DstWordOffset = 0;
	uint32 BitCountToCopy = BitCount;

	// We can adjust the final bit position here as we're only using the above variables from here on
	BufferBitPosition += BitCount;
	// Make sure PendingWord is up to date unless we've reached the end of the stream
	if (BufferBitPosition < BufferBitCapacity)
	{
		PendingWord = INTEL_ORDER32(Src[BufferBitPosition >> 5U]);
	}

	// Copy full words
	if (BitCountToCopy >= 32U)
	{
		// Fast path for byte aligned source buffer.
		if ((CurSrcBit & 7) == 0)
		{
			const uint32 WordCountToCopy = BitCountToCopy >> 5U;
			FPlatformMemory::Memcpy(Dst, reinterpret_cast<const uint8*>(Src) + (CurSrcBit >> 3U), WordCountToCopy*sizeof(uint32));
			DstWordOffset += WordCountToCopy;
		}
		else
		{
			// We know that each 32 bit copy straddles two words from Src as CurSrcBit % 32 != 0, 
			// else the fast path above would be used.
			const uint32 PrevWordShift = CurSrcBit & 31U;
			const uint32 NextWordShift = (32U - CurSrcBit) & 31U;

			// Set up initial Word so we can do a single read in each loop iteration.
			uint32 SrcWordOffset = CurSrcBit >> 5U;
			uint32 PrevWord = INTEL_ORDER32(Src[SrcWordOffset]);
			++SrcWordOffset;
			for (uint32 WordIt = 0, WordEndIt = (BitCountToCopy >> 5U); WordIt != WordEndIt; ++WordIt, ++SrcWordOffset, ++DstWordOffset)
			{
				const uint32 NextWord = INTEL_ORDER32(Src[SrcWordOffset]);
				const uint32 Word = (NextWord << NextWordShift) | (PrevWord >> PrevWordShift);
				Dst[DstWordOffset] = INTEL_ORDER32(Word);
				PrevWord = NextWord;
			}
		}

		const uint32 BitCountCopied = (BitCountToCopy & ~31U);
		CurSrcBit += BitCountCopied;
		BitCountToCopy &= 31U;
	}

	if (BitCountToCopy)
	{
		const uint32 Word = INTEL_ORDER32(Dst[DstWordOffset]);
		const uint32 SrcWord = BitStreamUtils::GetBits(Src, CurSrcBit, BitCountToCopy);
		const uint32 SrcMask = (1U << BitCountToCopy) - 1U;
		const uint32 DstWord = (Word & ~SrcMask) | (SrcWord & SrcMask);
		Dst[DstWordOffset] = INTEL_ORDER32(DstWord);
	}
}

void FNetBitStreamReader::Seek(uint32 BitPosition)
{
	if (BitPosition > BufferBitCapacity)
	{
		OverflowBitCount = BitPosition - BufferBitCapacity;
		return;
	}

	OverflowBitCount = 0;

	BufferBitPosition = BitPosition;
	if ((BufferBitPosition & ~31U) < BufferBitCapacity)
	{
		PendingWord = INTEL_ORDER32(Buffer[BufferBitPosition >> 5U]);
	}
}

void FNetBitStreamReader::DoOverflow()
{
	if (OverflowBitCount == 0)
	{
		Seek(BufferBitCapacity + 1);
	}
}

}
