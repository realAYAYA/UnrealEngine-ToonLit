// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::Net
{

class FNetBitStreamReader
{
public:
	IRISCORE_API FNetBitStreamReader();
	IRISCORE_API ~FNetBitStreamReader();

	/**
	 * InitBits must be called before reading from the stream.
	 * @param Buffer The buffer must be at least 4-byte aligned.
	 * @param BitCount The number of bits that is allowed to be read from the buffer.
	 */
	IRISCORE_API void InitBits(const void* Buffer, uint32 BitCount);

	/**
	 * Reads BitCount bits that are stored in the least significant bits in the return value. Other bits
	 * will be set to zero. If the BitCount exceeds the remaining space the function will return zero 
	 * and the stream will be marked as overflown.
	 */
	IRISCORE_API uint32 ReadBits(uint32 BitCount);

	/**
	 * Reads a bool from the stream and returns the value,
	 * A failed read will always return false and stream will be marked as overflown
	 */
	bool ReadBool() { return ReadBits(1) & 1U; }

	/**
	 * Reads BitCount bits and stores them in Dst, starting from bit offset 0. The bits will be stored
	 * as they are stored internally in this class, i.e. bits will be written from lower to higher
	 * memory addresses.
	 * If the BitCount exceeds the remaining space no bits will be written to Dst and the stream will be
	 * marked as overflown. It's up to the user to check for overflow.
	 */
	IRISCORE_API void ReadBitStream(uint32* Dst, uint32 BitCount);

	/**
	 * Seek to a specific BitPosition. If the stream is overflown and you seek back to a position
	 * where you can still read bits the stream will no longer be considered overflown.
	 */
	IRISCORE_API void Seek(uint32 BitPosition);

	/** Returns the current bit position. */
	inline uint32 GetPosBits() const { return BufferBitPosition; }

	/** Returns the number of bits that can be read before overflowing. */
	inline uint32 GetBitsLeft() const { return (OverflowBitCount ? 0U : (BufferBitCapacity - BufferBitPosition)); }

	/** Force an overflow. */
	IRISCORE_API void DoOverflow();
	
	/** Returns whether the stream is overflown or not. */
	inline bool IsOverflown() const { return OverflowBitCount != 0; }

private:
	const uint32* Buffer;
	uint32 BufferBitCapacity;
	uint32 BufferBitPosition;
	uint32 PendingWord;
	uint32 OverflowBitCount;
};

}

// Always report the actual bitstream position, even on overflow. This normally allows for better comparisons between sending and receiving side when bitstream errors occur.
inline uint32 GetBitStreamPositionForNetTrace(const UE::Net::FNetBitStreamReader& Stream) { return Stream.GetPosBits(); }
