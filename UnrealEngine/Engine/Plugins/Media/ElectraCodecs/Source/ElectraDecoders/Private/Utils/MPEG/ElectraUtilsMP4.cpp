// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/MPEG/ElectraUtilsMP4.h"


namespace ElectraDecodersUtil
{

FMP4AtomReader::FMP4AtomReader(const void* InDataPtr, int32 InDataSize)
	: DataPtr((const uint8*)InDataPtr), DataSize(InDataSize), CurrentOffset(0)
{
}

int32 FMP4AtomReader::GetCurrentOffset() const
{
	return CurrentOffset;	
}

int32 FMP4AtomReader::GetNumBytesRemaining() const
{ 
	return DataSize - GetCurrentOffset(); 
}

const uint8* FMP4AtomReader::GetCurrentDataPointer() const
{
	return GetNumBytesRemaining() ? DataPtr + GetCurrentOffset() : nullptr;
}

void FMP4AtomReader::SetCurrentOffset(int32 InNewOffset)
{
	check(InNewOffset >= 0 && InNewOffset <= DataSize);
	if (InNewOffset >= 0 && InNewOffset <= DataSize)
	{
		CurrentOffset = InNewOffset;
	}
}

bool FMP4AtomReader::ReadString(FString& OutString, uint16 NumBytes)
{
	OutString.Empty();
	if (NumBytes == 0)
	{
		return true;
	}
	TArray<uint8> Buf;
	Buf.AddUninitialized(NumBytes);
	if (ReadBytes(Buf.GetData(), NumBytes))
	{
		// Check for UTF16 BOM
		if (NumBytes >= 2 && ((Buf[0] == 0xff && Buf[1] == 0xfe) || (Buf[0] == 0xfe && Buf[1] == 0xff)))
		{
			// String uses UTF16, which is not supported
			return false;
		}
		FUTF8ToTCHAR cnv((const ANSICHAR*)Buf.GetData(), NumBytes);
		OutString = FString(cnv.Length(), cnv.Get());
		return true;
	}
	return false;
}

bool FMP4AtomReader::ReadStringUTF8(FString& OutString, int32 NumBytes)
{
	OutString.Empty();
	if (NumBytes == 0)
	{
		return true;
	}
	else if (NumBytes < 0)
	{
		NumBytes = GetNumBytesRemaining();
		check(NumBytes >= 0);
		if (NumBytes < 0)
		{
			return false;
		}
	}
	TArray<uint8> Buf;
	Buf.AddUninitialized(NumBytes);
	if (ReadBytes(Buf.GetData(), NumBytes))
	{
		FUTF8ToTCHAR cnv((const ANSICHAR*)Buf.GetData(), NumBytes);
		OutString = FString(cnv.Length(), cnv.Get());
		return true;
	}
	return false;
}

bool FMP4AtomReader::ReadStringUTF16(FString& OutString, int32 NumBytes)
{
	OutString.Empty();
	if (NumBytes == 0)
	{
		return true;
	}
	else if (NumBytes < 0)
	{
		NumBytes = GetNumBytesRemaining();
		check(NumBytes >= 0);
		if (NumBytes < 0)
		{
			return false;
		}
	}
	TArray<uint8> Buf;
	Buf.AddUninitialized(NumBytes);
	if (ReadBytes(Buf.GetData(), NumBytes))
	{
		check(!"TODO");
/*
		FUTF8ToTCHAR cnv((const ANSICHAR*)Buf.GetData(), NumBytes);
		OutString = FString(cnv.Length(), cnv.Get());
		return true;
*/
	}
	return false;
}

bool FMP4AtomReader::ReadAsNumber(uint64& OutValue, int32 InNumBytes)
{
	OutValue = 0;
	if (InNumBytes < 0 || InNumBytes > 8)
	{
		return false;
	}
	for(int32 i=0; i<InNumBytes; ++i)
	{
		uint8 d;
		if (!Read(d))
		{
			return false;
		}
		OutValue = (OutValue << 8) | d;
	}
	return true;
}
bool FMP4AtomReader::ReadAsNumber(int64& OutValue, int32 InNumBytes)
{
	OutValue = 0;
	if (InNumBytes < 0 || InNumBytes > 8)
	{
		return false;
	}
	for(int32 i=0; i<InNumBytes; ++i)
	{
		uint8 d;
		if (!Read(d))
		{
			return false;
		}
		if (i==0 && d>127)
		{
			OutValue = -1;
		}
		OutValue = (OutValue << 8) | d;
	}
	return true;
}
bool FMP4AtomReader::ReadAsNumber(float& OutValue)
{
	uint32 Flt;
	if (Read(Flt))
	{
		OutValue = *reinterpret_cast<float*>(&Flt);
		return true;
	}
	return false;
}
bool FMP4AtomReader::ReadAsNumber(double& OutValue)
{
	uint64 Dbl;
	if (Read(Dbl))
	{
		OutValue = *reinterpret_cast<double*>(&Dbl);
		return true;
	}
	return false;
}

bool FMP4AtomReader::ReadBytes(void* Buffer, int32 NumBytes)
{
	return ReadData(Buffer, NumBytes) == NumBytes;
}

int32 FMP4AtomReader::ReadData(void* IntoBuffer, int32 NumBytesToRead)
{
	if (NumBytesToRead <= 0)
	{
		return 0;
	}
	int32 NumAvail = DataSize - CurrentOffset;
	if (NumAvail >= NumBytesToRead)
	{
		if (IntoBuffer)
		{
			FMemory::Memcpy(IntoBuffer, DataPtr + CurrentOffset, NumBytesToRead);
		}
		CurrentOffset += NumBytesToRead;
		return NumBytesToRead;
	}
	return -1;
}


} // namespace ElectraDecodersUtil
