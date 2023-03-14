// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCStream.h"

#include "OSCLog.h"


namespace OSC
{
	const int32 DefaultWriteStreamSize = 1024;
} // namespace OSC

FOSCStream::FOSCStream()
	: Position(0)
	, bIsReadStream(false)
{
	// Set default size to reasonable size to
	// avoid thrashing with allocations while writing
	Data.Reserve(OSC::DefaultWriteStreamSize);
}

FOSCStream::FOSCStream(const uint8* InData, int32 InSize)
	: Data(InData, InSize)
	, Position(0)
	, bIsReadStream(true)
{
}

const uint8* FOSCStream::GetData() const
{
	return Data.GetData();
}

int32 FOSCStream::GetLength() const
{
	return Data.Num();
}

bool FOSCStream::HasReachedEnd() const
{
	return Position >= Data.Num();
}

int32 FOSCStream::GetPosition() const
{
	return Position;
}

void FOSCStream::SetPosition(int32 InPosition)
{
	check(InPosition <= Data.Num());
	Position = InPosition;
}

TCHAR FOSCStream::ReadChar()
{
	uint8 Temp;
	if (Read(&Temp, 1) > 0)
	{
		UE_CLOG(Temp > 0x7F, LogOSC, Warning, TEXT("Non-ANSI character '%u' written to OSCStream"), Temp);
		return (TCHAR)Temp;
	}

	return '\0';
}

void FOSCStream::WriteChar(TCHAR Char)
{
	const uint32 Temp = FChar::ToUnsigned(Char);
	UE_CLOG(Temp > 0x7F, LogOSC, Warning, TEXT("Non-ANSI character '%u' written to OSCStream"), Temp);
	const uint8 Temp8 = (uint8)Temp;
	Write(&Temp8, 1);
}

FColor FOSCStream::ReadColor()
{
	uint32 Packed = static_cast<uint32>(ReadInt32());
	return FColor(Packed);
}

void FOSCStream::WriteColor(FColor Color)
{
#if PLATFORM_LITTLE_ENDIAN
	uint32 Packed = Color.ToPackedABGR();
#else // PLATFORM_LITTLE_ENDIAN
	uint32 Packed = Color.ToPackedRGBA();
#endif // !PLATFORM_LITTLE_ENDIAN

	WriteInt32(static_cast<int32>(Packed));
}

int32 FOSCStream::ReadInt32()
{
	uint8 Temp[4];
	if (Read(Temp, 4) == 4)
	{
#if PLATFORM_LITTLE_ENDIAN
		union {
			int32 i;
			uint8 c[4];
		} u;

		u.c[0] = Temp[3];
		u.c[1] = Temp[2];
		u.c[2] = Temp[1];
		u.c[3] = Temp[0];

		return u.i;
#else
		return *(int32*)(Temp);
#endif // !PLATFORM_LITTLE_ENDIAN
	}

	return 0;
}

void FOSCStream::WriteInt32(int32 Value)
{
	uint8 Temp[4];

#ifdef PLATFORM_LITTLE_ENDIAN
	union {
		int32 i;
		uint8 c[4];
	} u;

	u.i = Value;

	Temp[3] = u.c[0];
	Temp[2] = u.c[1];
	Temp[1] = u.c[2];
	Temp[0] = u.c[3];
#else // PLATFORM_LITTLE_ENDIAN
	*(int32*)(Temp) = Value;
#endif // !PLATFORM_LITTLE_ENDIAN
	Write(Temp, 4);
}

double FOSCStream::ReadDouble()
{
	uint8 Temp[8];
	if (Read(Temp, 8) == 8)
	{
#if PLATFORM_LITTLE_ENDIAN
		union {
			double d;
			uint8 c[8];
		} u;

		u.c[0] = Temp[7];
		u.c[1] = Temp[6];
		u.c[2] = Temp[5];
		u.c[3] = Temp[4];
		u.c[4] = Temp[3];
		u.c[5] = Temp[2];
		u.c[6] = Temp[1];
		u.c[7] = Temp[0];

		return u.d;
#else // PLATFORM_LITTLE_ENDIAN
		return *(double*)Temp;
#endif // !PLATFORM_LITTLE_ENDIAN

	}

	return 0;
}

void FOSCStream::WriteDouble(uint64 Value)
{
	uint8 Temp[8];

#ifdef PLATFORM_LITTLE_ENDIAN
	union {
		double i;
		uint8 c[8];
	} u;

	u.i = Value;

	Temp[7] = u.c[0];
	Temp[6] = u.c[1];
	Temp[5] = u.c[2];
	Temp[4] = u.c[3];
	Temp[3] = u.c[4];
	Temp[2] = u.c[5];
	Temp[1] = u.c[6];
	Temp[0] = u.c[7];
#else // PLATFORM_LITTLE_ENDIAN
	*(double*)(Temp) = Value;
#endif // !PLATFORM_LITTLE_ENDIAN
	Write(Temp, 8);
}

int64 FOSCStream::ReadInt64()
{
	uint8 Temp[8];
	if (Read(Temp, 8) == 8)
	{
#if PLATFORM_LITTLE_ENDIAN
		union {
			int64 i;
			uint8 c[8];
		} u;

		u.c[0] = Temp[7];
		u.c[1] = Temp[6];
		u.c[2] = Temp[5];
		u.c[3] = Temp[4];
		u.c[4] = Temp[3];
		u.c[5] = Temp[2];
		u.c[6] = Temp[1];
		u.c[7] = Temp[0];

		return u.i;
#else // PLATFORM_LITTLE_ENDIAN
		return *(int64*)Temp;
#endif // !PLATFORM_LITTLE_ENDIAN
	}

	return 0;
}

void FOSCStream::WriteInt64(int64 Value)
{
	uint8 Temp[8];

#ifdef PLATFORM_LITTLE_ENDIAN
	union {
		int64 i;
		uint8 c[8];
	} u;

	u.i = Value;

	Temp[7] = u.c[0];
	Temp[6] = u.c[1];
	Temp[5] = u.c[2];
	Temp[4] = u.c[3];
	Temp[3] = u.c[4];
	Temp[2] = u.c[5];
	Temp[1] = u.c[6];
	Temp[0] = u.c[7];
#else // PLATFORM_LITTLE_ENDIAN
	*(int64*)(Temp) = Value;
#endif // !PLATFORM_LITTLE_ENDIAN
	Write(Temp, 8);
}

uint64 FOSCStream::ReadUInt64()
{
	uint8 Temp[8];
	if (Read(Temp, 8) == 8)
	{
#if PLATFORM_LITTLE_ENDIAN
		union {
			uint64 i;
			uint8 c[8];
		} u;

		u.c[0] = Temp[7];
		u.c[1] = Temp[6];
		u.c[2] = Temp[5];
		u.c[3] = Temp[4];
		u.c[4] = Temp[3];
		u.c[5] = Temp[2];
		u.c[6] = Temp[1];
		u.c[7] = Temp[0];

		return u.i;
#else // PLATFORM_LITTLE_ENDIAN
		return *(uint64*)Temp;
#endif // !PLATFORM_LITTLE_ENDIAN

	}

	return 0;
}

void FOSCStream::WriteUInt64(uint64 Value)
{
	uint8 Temp[8];

#ifdef PLATFORM_LITTLE_ENDIAN
	union {
		uint64 i;
		uint8 c[8];
	} u;

	u.i = Value;

	Temp[7] = u.c[0];
	Temp[6] = u.c[1];
	Temp[5] = u.c[2];
	Temp[4] = u.c[3];
	Temp[3] = u.c[4];
	Temp[2] = u.c[5];
	Temp[1] = u.c[6];
	Temp[0] = u.c[7];
#else // PLATFORM_LITTLE_ENDIAN
	*reinterpret_cast<uint64*>(Temp) = Value;
#endif // !PLATFORM_LITTLE_ENDIAN
	Write(Temp, 8);
}

float FOSCStream::ReadFloat()
{
	uint8 Temp[4];
	if (Read(Temp, 4) == 4)
	{
#if PLATFORM_LITTLE_ENDIAN
		union {
			float f;
			uint8 c[4];
		} u;

		u.c[0] = Temp[3];
		u.c[1] = Temp[2];
		u.c[2] = Temp[1];
		u.c[3] = Temp[0];

		return u.f;
#else // PLATFORM_LITTLE_ENDIAN
		return *(float*)Temp;
#endif // !PLATFORM_LITTLE_ENDIAN
	}

	return 0.0f;
}

void FOSCStream::WriteFloat(float Value)
{
	uint8 Temp[4];

#ifdef PLATFORM_LITTLE_ENDIAN
	union {
		float f;
		uint8 c[4];
	} u;

	u.f = Value;

	Temp[3] = u.c[0];
	Temp[2] = u.c[1];
	Temp[1] = u.c[2];
	Temp[0] = u.c[3];
#else // if !PLATFORM_LITTLE_ENDIAN
	*(float*)(Temp) = Value;
#endif // !PLATFORM_LITTLE_ENDIAN

	Write(Temp, 4);
}

FString FOSCStream::ReadString()
{
	const int32 DataSize = Data.Num();
	if (HasReachedEnd() || DataSize == 0)
	{
		return FString();
	}

	// Cache init position index and push along until either at end or character read is null terminator.
	const int32 InitPosition = GetPosition();
	while (!HasReachedEnd() && ReadChar() != '\0');

	if (HasReachedEnd() && Data.Last() != '\0')
	{
		UE_LOG(LogOSC, Error, TEXT("Invalid string when reading OSCStream: Null terminator '\0' not found"));
		return FString();
	}

	// Cache end for string copy, increment to next read
	// location, and consume pad until next 4-byte boundary.
	const int32 EndPosition = Position;

	// Count includes the null terminator.
	const int32 Count = EndPosition - InitPosition;
	check(Count > 0);

	const int32 UnboundByteCount = Count % 4;
	if (UnboundByteCount != 0)
	{
		Position += 4 - UnboundByteCount;
	}

	// Exclude null terminator here; this constructor appends one.
	return FString(Count - 1, (ANSICHAR*)(&Data[InitPosition]));
}

void FOSCStream::WriteString(const FString& InString)
{
	const TArray<TCHAR, FString::AllocatorType>& CharArr = InString.GetCharArray();

	int32 Count = CharArr.Num();
	if (Count == 0)
	{
		WriteChar('\0');
		Count++;
	}
	else
	{
		for (int32 i = 0; i < Count; i++)
		{
			WriteChar(CharArr[i]);
		}
	}

	// Increment & pad string with null terminator (String must
	// be packed into multiple of 4 bytes)
	check(Count > 0);
	const int32 UnboundByteCount = Count % 4;
	if (UnboundByteCount != 0)
	{
		const int32 NumPaddingZeros = 4 - UnboundByteCount;
		for (int32 i = 0; i < NumPaddingZeros; i++)
		{
			WriteChar('\0');
		}
	}
}

TArray<uint8> FOSCStream::ReadBlob()
{
	TArray<uint8> Blob;

	const int32 BlobSize = ReadInt32();
	for (int32 i = 0; i < BlobSize; i++)
	{
		Blob.Add(ReadChar());
	}

	Position = ((Position + 3) / 4) * 4; // padded

	return Blob;
}

void FOSCStream::WriteBlob(TArray<uint8>& Blob)
{
	WriteInt32(Blob.Num());
	for (int32 i = 0; i < Blob.Num(); i++)
	{
		Write(&Blob[i], 4u);
	}
}

int32 FOSCStream::Read(uint8* Buffer, int32 InSize)
{
	check(bIsReadStream);
	const int32 DataSize = Data.Num();
	if (InSize == 0 || Position >= DataSize)
	{
		return 0;
	}

	const int32 Num = FMath::Min<int32>(InSize, DataSize - Position);
	if (Num > 0)
	{
		FMemory::Memcpy(Buffer, &Data[Position], Num);
		Position += Num;
	}

	return Num;
}

int32 FOSCStream::Write(const uint8* InBuffer, int32 InSize)
{
	check(!bIsReadStream);
	if (InSize <= 0)
	{
		return 0;
	}

	if (Position < Data.Num())
	{
		int32 Slack = Data.Num() - Position;
		if (InSize - Slack > 0)
		{
			Data.AddUninitialized(InSize - Slack);
		}
	}
	else
	{
		check(Position == Data.Num());
		Data.AddUninitialized(InSize);
	}

	FMemory::Memcpy(&Data[Position], InBuffer, InSize);
	Position += InSize;
	return InSize;
}

