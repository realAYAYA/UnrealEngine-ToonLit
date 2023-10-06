// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

namespace ElectraDecodersUtil
{
#if !PLATFORM_LITTLE_ENDIAN
	static inline uint8 GetFromBigEndian(uint8 value)		{ return value; }
	static inline int8 GetFromBigEndian(int8 value)			{ return value; }
	static inline uint16 GetFromBigEndian(uint16 value)		{ return value; }
	static inline int16 GetFromBigEndian(int16 value)		{ return value; }
	static inline int32 GetFromBigEndian(int32 value)		{ return value; }
	static inline uint32 GetFromBigEndian(uint32 value)		{ return value; }
	static inline int64 GetFromBigEndian(int64 value)		{ return value; }
	static inline uint64 GetFromBigEndian(uint64 value)		{ return value; }
#else
	static inline uint16 EndianSwap(uint16 value)			{ return (value >> 8) | (value << 8); }
	static inline int16 EndianSwap(int16 value)				{ return int16(EndianSwap(uint16(value))); }
	static inline uint32 EndianSwap(uint32 value)			{ return (value << 24) | ((value & 0xff00) << 8) | ((value >> 8) & 0xff00) | (value >> 24); }
	static inline int32 EndianSwap(int32 value)				{ return int32(EndianSwap(uint32(value))); }
	static inline uint64 EndianSwap(uint64 value)			{ return (uint64(EndianSwap(uint32(value & 0xffffffffU))) << 32) | uint64(EndianSwap(uint32(value >> 32))); }
	static inline int64 EndianSwap(int64 value)				{ return int64(EndianSwap(uint64(value)));}
	static inline uint8 GetFromBigEndian(uint8 value)		{ return value; }
	static inline int8 GetFromBigEndian(int8 value)			{ return value; }
	static inline uint16 GetFromBigEndian(uint16 value)		{ return EndianSwap(value); }
	static inline int16 GetFromBigEndian(int16 value)		{ return EndianSwap(value); }
	static inline int32 GetFromBigEndian(int32 value)		{ return EndianSwap(value); }
	static inline uint32 GetFromBigEndian(uint32 value)		{ return EndianSwap(value); }
	static inline int64 GetFromBigEndian(int64 value)		{ return EndianSwap(value); }
	static inline uint64 GetFromBigEndian(uint64 value)		{ return EndianSwap(value); }
#endif

	static constexpr uint32 MakeMP4Atom(const uint8 A, const uint8 B, const uint8 C, const uint8 D)
	{
		return (static_cast<uint32>(A) << 24) | (static_cast<uint32>(B) << 16) | (static_cast<uint32>(C) << 8) | static_cast<uint32>(D);
	}


	class ELECTRADECODERS_API FMP4AtomReader
	{
	public:
		FMP4AtomReader(const void* InDataPtr, int32 InDataSize);

		int32 GetCurrentOffset() const;
		int32 GetNumBytesRemaining() const;
		const uint8* GetCurrentDataPointer() const;
		void SetCurrentOffset(int32 InNewOffset);

		template <typename T>
		bool Read(T& value)
		{
			T Temp = 0;
			int64 NumRead = ReadData(&Temp, sizeof(T));
			if (NumRead == sizeof(T))
			{
				value = ValueFromBigEndian(Temp);
				return true;
			}
			return false;
		}

		bool ReadString(FString& OutString, uint16 NumBytes);
		bool ReadStringUTF8(FString& OutString, int32 NumBytes);
		bool ReadStringUTF16(FString& OutString, int32 NumBytes);
		bool ReadBytes(void* Buffer, int32 NumBytes);
		bool ReadAsNumber(int64& OutValue, int32 InNumBytes);
		bool ReadAsNumber(uint64& OutValue, int32 InNumBytes);
		bool ReadAsNumber(float& OutValue);
		bool ReadAsNumber(double& OutValue);

	private:
		template <typename T>
		T ValueFromBigEndian(const T value)
		{
			return GetFromBigEndian(value);
		}
		int32 ReadData(void* IntoBuffer, int32 NumBytesToRead);

		const uint8* DataPtr = nullptr;
		int32 DataSize = 0;
		int32 CurrentOffset = 0;
	};

} // namespace ElectraDecodersUtil
