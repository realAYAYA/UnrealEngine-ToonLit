// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/Base64.h"

#include "Containers/StringConv.h"
#include "Misc/AssertionMacros.h"

/** The table used to encode a 6 bit value as an ascii character */
static const uint8 EncodingAlphabet[64] = 
{ 
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' 
};

/** The table used to convert an ascii character into a 6 bit value */
static const uint8 DecodingAlphabet[256] = 
{ 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x00-0x0f
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x10-0x1f
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F, // 0x20-0x2f
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x30-0x3f
	0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, // 0x40-0x4f
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x50-0x5f
	0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // 0x60-0x6f
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x70-0x7f
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x80-0x8f
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x90-0x9f
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xa0-0xaf
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xb0-0xbf
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xc0-0xcf
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xd0-0xdf
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xe0-0xef
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // 0xf0-0xff
};

FString FBase64::Encode(const FString& Source)
{
	return Encode((uint8*)TCHAR_TO_ANSI(*Source), Source.Len());
}

FString FBase64::Encode(const TArray<uint8>& Source)
{
	return Encode((uint8*)Source.GetData(), Source.Num());
}

FString FBase64::Encode(const uint8* Source, uint32 Length)
{
	uint32 ExpectedLength = GetEncodedDataSize(Length);
	
	FString OutBuffer;

	TArray<TCHAR, FString::AllocatorType>& OutCharArray = OutBuffer.GetCharArray();
	OutCharArray.SetNum(ExpectedLength + 1);
	int64 EncodedLength = Encode(Source, Length, OutCharArray.GetData());
	verify(EncodedLength == OutBuffer.Len());

	return OutBuffer;
}

template<typename CharType> uint32 FBase64::Encode(const uint8* Source, uint32 Length, CharType* Dest)
{
	CharType* EncodedBytes = Dest;

	// Loop through the buffer converting 3 bytes of binary data at a time
	while (Length >= 3)
	{
		uint8 A = *Source++;
		uint8 B = *Source++;
		uint8 C = *Source++;
		Length -= 3;

		// The algorithm takes 24 bits of data (3 bytes) and breaks it into 4 6bit chunks represented as ascii
		uint32 ByteTriplet = A << 16 | B << 8 | C;

		// Use the 6bit block to find the representation ascii character for it
		EncodedBytes[3] = EncodingAlphabet[ByteTriplet & 0x3F];
		ByteTriplet >>= 6;
		EncodedBytes[2] = EncodingAlphabet[ByteTriplet & 0x3F];
		ByteTriplet >>= 6;
		EncodedBytes[1] = EncodingAlphabet[ByteTriplet & 0x3F];
		ByteTriplet >>= 6;
		EncodedBytes[0] = EncodingAlphabet[ByteTriplet & 0x3F];

		// Now we can append this buffer to our destination string
		EncodedBytes += 4;
	}

	// Since this algorithm operates on blocks, we may need to pad the last chunks
	if (Length > 0)
	{
		uint8 A = *Source++;
		uint8 B = 0;
		uint8 C = 0;
		// Grab the second character if it is a 2 uint8 finish
		if (Length == 2)
		{
			B = *Source;
		}
		uint32 ByteTriplet = A << 16 | B << 8 | C;
		// Pad with = to make a 4 uint8 chunk
		EncodedBytes[3] = '=';
		ByteTriplet >>= 6;
		// If there's only one 1 uint8 left in the source, then you need 2 pad chars
		if (Length == 1)
		{
			EncodedBytes[2] = '=';
		}
		else
		{
			EncodedBytes[2] = EncodingAlphabet[ByteTriplet & 0x3F];
		}
		// Now encode the remaining bits the same way
		ByteTriplet >>= 6;
		EncodedBytes[1] = EncodingAlphabet[ByteTriplet & 0x3F];
		ByteTriplet >>= 6;
		EncodedBytes[0] = EncodingAlphabet[ByteTriplet & 0x3F];

		EncodedBytes += 4;
	}

	// Add a null terminator
	*EncodedBytes = 0;

	return UE_PTRDIFF_TO_UINT32(EncodedBytes - Dest);
}

template CORE_API uint32 FBase64::Encode<ANSICHAR>(const uint8* Source, uint32 Length, ANSICHAR* Dest);
template CORE_API uint32 FBase64::Encode<WIDECHAR>(const uint8* Source, uint32 Length, WIDECHAR* Dest);

bool FBase64::Decode(const FString& Source, FString& OutDest)
{
	uint32 ExpectedLength = GetDecodedDataSize(Source);

	TArray<ANSICHAR> TempDest;
	TempDest.AddZeroed(ExpectedLength + 1);
	if(!Decode(*Source, Source.Len(), (uint8*)TempDest.GetData()))
	{
		return false;
	}
	OutDest = ANSI_TO_TCHAR(TempDest.GetData());
	return true;
}

bool FBase64::Decode(const FString& Source, TArray<uint8>& OutDest)
{
	OutDest.SetNum(GetDecodedDataSize(Source));
	return Decode(*Source, Source.Len(), OutDest.GetData());
}

template<typename CharType> bool FBase64::Decode(const CharType* Source, uint32 Length, uint8* Dest)
{
	// Remove the trailing '=' characters, so we can handle padded and non-padded input the same
	while(Length > 0 && Source[Length - 1] == '=')
	{
		Length--;
	}

	// Make sure the length is valid. Only lengths modulo 4 of 0, 2, and 3 are valid.
	if((Length & 3) == 1)
	{
		return false;
	}

	// Convert all the full chunks of data
	for(; Length >= 4; Length -= 4)
	{
		// Decode the next 4 BYTEs
		uint32 OriginalTriplet = 0;
		for (int32 Index = 0; Index < 4; Index++)
		{
			uint32 SourceChar = (uint32)*Source++;
			if(SourceChar >= 256)
			{
				return false;
			}
			uint8 DecodedValue = DecodingAlphabet[SourceChar];
			if (DecodedValue == 0xFF)
			{
				return false;
			}
			OriginalTriplet = (OriginalTriplet << 6) | DecodedValue;
		}

		// Now we can tear the uint32 into bytes
		Dest[2] = OriginalTriplet & 0xFF;
		OriginalTriplet >>= 8;
		Dest[1] = OriginalTriplet & 0xFF;
		OriginalTriplet >>= 8;
		Dest[0] = OriginalTriplet & 0xFF;

		// Move to the next output chunk
		Dest += 3;
	}

	// Convert the last chunk of data
	if(Length > 0)
	{
		// Decode the next 4 BYTEs, or up to the end of the input buffer
		uint32 OriginalTriplet = 0;
		for (uint32 Index = 0; Index < Length; Index++)
		{
			uint32 SourceChar = (uint32)*Source++;
			if(SourceChar >= 256)
			{
				return false;
			}
			uint8 DecodedValue = DecodingAlphabet[SourceChar];
			if (DecodedValue == 0xFF)
			{
				return false;
			}
			OriginalTriplet = (OriginalTriplet << 6) | DecodedValue;
		}
		for(int32 Index = Length; Index < 4; Index++)
		{
			OriginalTriplet = (OriginalTriplet << 6);
		}

		// Now we can tear the uint32 into bytes
		OriginalTriplet >>= 8;
		if(Length >= 3)
		{
			Dest[1] = OriginalTriplet & 0xFF;
		}
		OriginalTriplet >>= 8;
		Dest[0] = OriginalTriplet & 0xFF;
	}
	return true;
}

template CORE_API bool FBase64::Decode<ANSICHAR>(const ANSICHAR* Source, uint32 Length, uint8* Dest);
template CORE_API bool FBase64::Decode<WIDECHAR>(const WIDECHAR* Source, uint32 Length, uint8* Dest);

uint32 FBase64::GetDecodedDataSize(const FString& Source)
{
	return GetDecodedDataSize(*Source, Source.Len());
}

template<typename CharType> uint32 FBase64::GetDecodedDataSize(const CharType* Source, uint32 Length)
{
	uint32 NumBytes = 0;
	if(Length > 0)
	{
		// Get the source length without the trailing padding characters
		while(Length > 0 && Source[Length - 1] == '=')
		{
			Length--;
		}

		// Get the lower bound for number of bytes, excluding the last partial chunk
		NumBytes += (Length / 4) * 3;
		if((Length & 3) == 3)
		{
			NumBytes += 2;
		}
		if((Length & 3) == 2)
		{
			NumBytes++;
		}
	}
	return NumBytes;
}

template CORE_API uint32 FBase64::GetDecodedDataSize(const ANSICHAR* Source, uint32 Length);
template CORE_API uint32 FBase64::GetDecodedDataSize(const WIDECHAR* Source, uint32 Length);
