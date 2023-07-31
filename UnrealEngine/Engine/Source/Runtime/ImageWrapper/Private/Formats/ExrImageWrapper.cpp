// Copyright Epic Games, Inc. All Rights Reserved.

#include "Formats/ExrImageWrapper.h"
#include "ImageWrapperPrivate.h"

#include "Containers/StringConv.h"
#include "HAL/PlatformTime.h"
#include "Math/Float16.h"

/**

@todo Oodle : this is a flawed EXR importer
it crashes on many images in the OpenEXR test set
Blobbies.exr and spirals.exr among others

**/

#if WITH_UNREALEXR

FExrImageWrapper::FExrImageWrapper()
	: FImageWrapperBase()
{
}

class FMemFileOut : public Imf::OStream
{
public:
	//-------------------------------------------------------
	// A constructor that opens the file with the given name.
	// The destructor will close the file.
	//-------------------------------------------------------

	FMemFileOut(const char fileName[]) :
		Imf::OStream(fileName),
		Pos(0)
	{
	}

	// InN must be 32bit to match the abstract interface.
	virtual void write(const char c[/*n*/], int32 InN)
	{
		int64 SrcN = (int64)InN;
		int64 DestPost = Pos + SrcN;
		if (DestPost > Data.Num())
		{
			Data.AddUninitialized(FMath::Max(Data.Num() * 2, DestPost) - Data.Num());
		}

		for (int64 i = 0; i < SrcN; ++i)
		{
			Data[Pos + i] = c[i];
		}
		Pos += SrcN;
	}


	//---------------------------------------------------------
	// Get the current writing position, in bytes from the
	// beginning of the file.  If the next call to write() will
	// start writing at the beginning of the file, tellp()
	// returns 0.
	//---------------------------------------------------------

	uint64_t tellp() override
	{
		return Pos;
	}


	//-------------------------------------------
	// Set the current writing position.
	// After calling seekp(i), tellp() returns i.
	//-------------------------------------------

	void seekp(uint64_t pos) override
	{
		Pos = pos;
	}


	int64 Pos;
	TArray64<uint8> Data;
};


class FMemFileIn : public Imf::IStream
{
public:
	//-------------------------------------------------------
	// A constructor that opens the file with the given name.
	// The destructor will close the file.
	//-------------------------------------------------------

	FMemFileIn(const void* InData, int64 InSize)
		: Imf::IStream("")
		, Data((const char *)InData)
		, Size(InSize)
		, Pos(0)
	{
	}

	//------------------------------------------------------
	// Read from the stream:
	//
	// read(c,n) reads n bytes from the stream, and stores
	// them in array c.  If the stream contains less than n
	// bytes, or if an I/O error occurs, read(c,n) throws
	// an exception.  If read(c,n) reads the last byte from
	// the file it returns false, otherwise it returns true.
	//------------------------------------------------------

	// InN must be 32bit to match the abstract interface.
	virtual bool read (char c[/*n*/], int32 InN)
	{
		int64 SrcN = InN;

		if(Pos + SrcN > Size)
		{
			return false;
		}

		for (int64 i = 0; i < SrcN; ++i)
		{
			c[i] = Data[Pos];
			++Pos;
		}

		return Pos >= Size;
	}

	//--------------------------------------------------------
	// Get the current reading position, in bytes from the
	// beginning of the file.  If the next call to read() will
	// read the first byte in the file, tellg() returns 0.
	//--------------------------------------------------------

	uint64_t tellg() override
	{
		return Pos;
	}

	//-------------------------------------------
	// Set the current reading position.
	// After calling seekg(i), tellg() returns i.
	//-------------------------------------------

	void seekg(uint64_t pos) override
	{
		Pos = pos;
	}

private:

	const char* Data;
	int64 Size;
	int64 Pos;
};

const char* cChannelNamesRGBA[] = { "R", "G", "B", "A" };
const char* cChannelNamesBGRA[] = { "B", "G", "R", "A" };
//const char* cChannelNamesGray[] = { "G" }; // is that green or gray ?
const char* cChannelNamesGray[] = { "Y" }; // pretty sure "Y" is more standard for gray

int32 GetChannelNames(ERGBFormat InRGBFormat, const char* const*& OutChannelNames)
{
	int32 ChannelCount;

	switch (InRGBFormat)
	{
	case ERGBFormat::RGBA:
	case ERGBFormat::RGBAF:
		OutChannelNames = cChannelNamesRGBA;
		ChannelCount = UE_ARRAY_COUNT(cChannelNamesRGBA);
		break;

	case ERGBFormat::BGRA:
		OutChannelNames = cChannelNamesBGRA;
		ChannelCount = UE_ARRAY_COUNT(cChannelNamesBGRA);
		break;

	case ERGBFormat::Gray:
	case ERGBFormat::GrayF:
		OutChannelNames = cChannelNamesGray;
		ChannelCount = UE_ARRAY_COUNT(cChannelNamesGray);
		break;

	default:
		OutChannelNames = nullptr;
		ChannelCount = 0;
	}

	return ChannelCount;
}


bool FExrImageWrapper::CanSetRawFormat(const ERGBFormat InFormat, const int32 InBitDepth) const
{
	return (InFormat == ERGBFormat::RGBAF || InFormat == ERGBFormat::GrayF) && (InBitDepth == 16 || InBitDepth == 32);
}

ERawImageFormat::Type FExrImageWrapper::GetSupportedRawFormat(const ERawImageFormat::Type InFormat) const
{
	switch(InFormat)
	{
	case ERawImageFormat::RGBA16F:
	case ERawImageFormat::RGBA32F:
	case ERawImageFormat::R16F:
	case ERawImageFormat::R32F:
		return InFormat; // directly supported
	case ERawImageFormat::G8:
		return ERawImageFormat::R16F; // needs conversion
	case ERawImageFormat::BGRA8:
	case ERawImageFormat::BGRE8:
		return ERawImageFormat::RGBA16F; // needs conversion
	case ERawImageFormat::G16:
	case ERawImageFormat::RGBA16:
		return ERawImageFormat::RGBA32F; // needs conversion
	default:
		check(0);
		return ERawImageFormat::BGRA8;
	}
}

bool FExrImageWrapper::SetRaw(const void* InRawData, int64 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth, const int32 InBytesPerRow)
{
	check(InRawData);
	check(InRawSize > 0);
	check(InWidth > 0);
	check(InHeight > 0);
	check(InBytesPerRow >= 0);

	// FExrImageWrapper used to take RGBA 8-bit input
	//	 and write it linearly
	// the new image path now requires you to convert to float before coming in here
	// so U8 will be converted to float *with* gamma correction

	switch (InBitDepth)
	{
	case 8:
		if (InFormat != ERGBFormat::RGBA && InFormat != ERGBFormat::BGRA && InFormat != ERGBFormat::Gray)
		{
			return false;
		}
		break;

	case 16:
	case 32:
		if (InFormat == ERGBFormat::RGBA || InFormat == ERGBFormat::Gray)
		{
			// Before ERGBFormat::RGBAF and ERGBFormat::GrayF were introduced, ERGBFormat::RGBA and ERGBFormat::Gray were used to describe float pixel formats.
			// ERGBFormat::RGBA and ERGBFormat::Gray should now only be used for integer channels.
			// Note that EXR uint32 compression is currently not supported.
			const TCHAR* FormatName = (InFormat == ERGBFormat::RGBA) ? TEXT("RGBA") : TEXT("Gray");
			UE_LOG(LogImageWrapper, Warning, TEXT("Usage of 16-bit and 32-bit ERGBFormat::%s raw format for compressing EXR images is deprecated, if you are compressing float channels please specify ERGBFormat::%sF instead."), *FormatName, *FormatName);
		}
		if (InFormat != ERGBFormat::RGBAF && InFormat != ERGBFormat::GrayF)
		{
			return false;
		}
		break;
	}

	return FImageWrapperBase::SetRaw(InRawData, InRawSize, InWidth, InHeight, InFormat, InBitDepth, InBytesPerRow);
}

bool FExrImageWrapper::SetCompressed(const void* InCompressedData, int64 InCompressedSize)
{
	check(InCompressedData);
	check(InCompressedSize > 0);

	// Check the magic value in advance to avoid spamming the log with EXR parsing errors.
	if (InCompressedSize < sizeof(uint32) || *(uint32*)InCompressedData != Imf::MAGIC)
	{
		return false;
	}

	if (!FImageWrapperBase::SetCompressed(InCompressedData, InCompressedSize))
	{
		return false;
	}

	// openEXR can throw exceptions when parsing invalid data.
	try
	{
		FMemFileIn MemFile(CompressedData.GetData(), CompressedData.Num());
		Imf::InputFile ImfFile(MemFile);
		Imf::Header ImfHeader = ImfFile.header();
		Imf::ChannelList ImfChannels = ImfHeader.channels();
		Imath::Box2i ImfDataWindow = ImfHeader.dataWindow();
		Width = ImfDataWindow.max.x - ImfDataWindow.min.x + 1;
		Height = ImfDataWindow.max.y - ImfDataWindow.min.y + 1;

		bool bHasOnlyHALFChannels = true;
		bool bHasAlphaChannel = false;
		int32 ChannelCount = 0;

		for (Imf::ChannelList::Iterator Iter = ImfChannels.begin(); Iter != ImfChannels.end(); ++Iter)
		{
			++ChannelCount;

			bHasOnlyHALFChannels = bHasOnlyHALFChannels && Iter.channel().type == Imf::HALF;

			// check for bMatchesGrayOrder disabled
			// treat any 1-channel import as gray
			// don't try to match the channel names, which are not standardized
			//  (we incorrectly used "G" before, we now use "Y", TinyEXR uses "A" for 1-channel EXR)
			//bMatchesGrayOrder = bMatchesGrayOrder && ChannelCount < UE_ARRAY_COUNT(cChannelNamesGray) && !strcmp(Iter.name(), cChannelNamesGray[ChannelCount]);

			if ( strcmp(Iter.name(),"A") == 0 )
			{
				bHasAlphaChannel = true;
			}
		}

		BitDepth = (ChannelCount && bHasOnlyHALFChannels) ? 16 : 32;

		// EXR uint32 channels are currently not supported, therefore input channels are always treated as float channels.
		// Channel combinations which don't match the ERGBFormat::GrayF pattern are qualified as ERGBFormat::RGBAF.
		// Note that channels inside the EXR file are indexed by name, therefore can be decoded in any RGB order.

		// NOTE: TinyEXR writes 1-channel EXR as an "A" named channel
		//  this cannot be loaded as a 1-channel image (you would just get all zeros)
		// it must be loaded as RGBA here
		// @todo Oodle : load that as RGBA then move A to R and convert back to 1 channel ?
		if (ChannelCount == 1 && !bHasAlphaChannel)
		{
			Format = ERGBFormat::GrayF;
		}
		else
		{
			Format = ERGBFormat::RGBAF;
		}
	}
	catch (const std::exception& Exception)
	{
		TStringConversion<TStringConvert<char, TCHAR>> Convertor(Exception.what());
		UE_LOG(LogImageWrapper, Error, TEXT("Cannot parse EXR image header: %s"), Convertor.Get());
		SetError(Convertor.Get());
		return false;
	}

	return true;
}

void FExrImageWrapper::Compress(int32 Quality)
{
	check(RawData.Num());

	// Ensure we haven't already compressed the file.
	if (CompressedData.Num())
	{
		return;
	}

	const double StartTime = FPlatformTime::Seconds();

	Imf::PixelType ImfPixelType;
	TArray64<uint8> ConvertedRawData;
	bool bNeedsConversion = false;

	if (BitDepth == 8)
	{
		// uint8 channels are linearly converted into FFloat16 channels.
		// note: NO GAMMA CORRECTION
		ConvertedRawData.SetNumUninitialized(sizeof(FFloat16) * RawData.Num());
		FFloat16* Output = reinterpret_cast<FFloat16*>(ConvertedRawData.GetData());
		for (int64 i = 0; i < RawData.Num(); ++i)
		{
			Output[i] = FFloat16(RawData[i] / 255.f);
		}
		ImfPixelType = Imf::HALF;
		bNeedsConversion = true;
	}
	else
	{
		ImfPixelType = (BitDepth == 16) ? Imf::HALF : Imf::FLOAT;
	}

	const TArray64<uint8>& PixelData = bNeedsConversion ? ConvertedRawData : RawData;

	const char* const* ChannelNames;
	int32 ChannelCount = GetChannelNames(Format, ChannelNames);
	check((int64)ChannelCount * Width * Height * BitDepth == RawData.Num() * 8);

	int32 BytesPerChannelPixel = (ImfPixelType == Imf::HALF) ? 2 : 4;
	TArray<TArray64<uint8>> ChannelData;
	ChannelData.SetNum(ChannelCount);

	for (int32 c = 0; c < ChannelCount; ++c)
	{
		ChannelData[c].SetNumUninitialized((int64)BytesPerChannelPixel * Width * Height);
	}

	// EXR channels are compressed non-interleaved.
	for (int64 OffsetNonInterleaved = 0, OffsetInterleaved = 0; OffsetInterleaved < PixelData.Num(); OffsetNonInterleaved += BytesPerChannelPixel)
	{
		for (int32 c = 0; c < ChannelCount; ++c)
		{
			for (int32 b = 0; b < BytesPerChannelPixel; ++b, ++OffsetInterleaved)
			{
				ChannelData[c][OffsetNonInterleaved + b] = PixelData[OffsetInterleaved];
			}
		}
	}

	Imf::Compression ImfCompression = (Quality == (int32)EImageCompressionQuality::Uncompressed) ? Imf::Compression::NO_COMPRESSION : Imf::Compression::ZIP_COMPRESSION;
	Imf::Header ImfHeader(Width, Height, 1, Imath::V2f(0, 0), 1, Imf::LineOrder::INCREASING_Y, ImfCompression);
	Imf::FrameBuffer ImfFrameBuffer;

	for (int32 c = 0; c < ChannelCount; ++c)
	{
		ImfHeader.channels().insert(ChannelNames[c], Imf::Channel(ImfPixelType));
		ImfFrameBuffer.insert(ChannelNames[c], Imf::Slice(ImfPixelType, (char*)ChannelData[c].GetData(), BytesPerChannelPixel, (size_t)BytesPerChannelPixel * Width));
	}

	FMemFileOut MemFile("");
	int64 MemFileLength;

	{
		// This scope ensures that IMF::Outputfile creates a complete file by closing the file when it goes out of scope.
		// To complete the file, EXR seeks back into the file and writes the scanline offsets when the file is closed, 
		// which moves the tellp location. So file length is stored in advance for later use.

		Imf::OutputFile ImfFile(MemFile, ImfHeader, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
		ImfFile.setFrameBuffer(ImfFrameBuffer);
		ImfFile.writePixels(Height);
		MemFileLength = MemFile.tellp();
	}

	CompressedData = MoveTemp(MemFile.Data);
	CompressedData.SetNum(MemFileLength);

	const double DeltaTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogImageWrapper, Verbose, TEXT("Compressed image in %.3f seconds"), DeltaTime);
}

void FExrImageWrapper::Uncompress(const ERGBFormat InFormat, const int32 InBitDepth)
{
	check(CompressedData.Num());

	// Ensure we haven't already uncompressed the file.
	if (RawData.Num() && InFormat == Format && InBitDepth == BitDepth)
	{
		return;
	}

	FString ErrorMessage;

	if (InBitDepth == 16 && (InFormat == ERGBFormat::RGBA || InFormat == ERGBFormat::Gray))
	{
		// Before ERGBFormat::RGBAF and ERGBFormat::GrayF were introduced, 16-bit ERGBFormat::RGBA and ERGBFormat::Gray were used to describe float pixel formats.
		// ERGBFormat::RGBA and ERGBFormat::Gray should now only be used for integer channels, while EXR format doesn't support 16-bit integer channels.
		const TCHAR* FormatName = (InFormat == ERGBFormat::RGBA) ? TEXT("RGBA") : TEXT("Gray");
		ErrorMessage = FString::Printf(TEXT("Usage of 16-bit ERGBFormat::%s raw format for decompressing float EXR channels is deprecated, please use ERGBFormat::%sF instead."), FormatName, FormatName);
	}
	else if (InBitDepth != 16 && InBitDepth != 32)
	{
		ErrorMessage = TEXT("Unsupported bit depth, expected 16 or 32.");
	}
	else if (InFormat != ERGBFormat::RGBAF && InFormat != ERGBFormat::GrayF)
	{
		// EXR uint32 channels are currently not supported
		ErrorMessage = TEXT("Unsupported RGB format, expected ERGBFormat::RGBAF or ERGBFormat::GrayF.");
	}

	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogImageWrapper, Error, TEXT("Cannot decompress EXR image: %s."), *ErrorMessage);
		SetError(*ErrorMessage);
		return;
	}

	const char* const* ChannelNames;
	int32 ChannelCount = GetChannelNames(InFormat, ChannelNames);
	check(ChannelCount == 1 || ChannelCount == 4);

	TArray<TArray64<uint8>> ChannelData;
	ChannelData.SetNum(ChannelCount);

	Imf::PixelType ImfPixelType = (InBitDepth == 16) ? Imf::HALF : Imf::FLOAT;
	int32 BytesPerChannelPixel = (ImfPixelType == Imf::HALF) ? 2 : 4;

	// openEXR can throw exceptions when parsing invalid data.
	try
	{
		Imf::FrameBuffer ImfFrameBuffer;
		for (int32 c = 0; c < ChannelCount; ++c)
		{
			ChannelData[c].SetNumUninitialized((int64)BytesPerChannelPixel * Width * Height);
			// Use 1.0 as a default value for the alpha channel, in case if it is not present in the EXR, use 0.0 for all other channels.
			double DefaultValue = !strcmp(ChannelNames[c], "A") ? 1.0 : 0.0;
			ImfFrameBuffer.insert(ChannelNames[c], Imf::Slice(ImfPixelType, (char*)ChannelData[c].GetData(), BytesPerChannelPixel, (size_t)BytesPerChannelPixel * Width, 1, 1, DefaultValue));
		}
		FMemFileIn MemFile(CompressedData.GetData(), CompressedData.Num());
		Imf::InputFile ImfFile(MemFile);
		Imf::Header ImfHeader = ImfFile.header();
		Imath::Box2i ImfDataWindow = ImfHeader.dataWindow();
		ImfFile.setFrameBuffer(ImfFrameBuffer);
		ImfFile.readPixels(ImfDataWindow.min.y, ImfDataWindow.max.y);
	}
	catch (const std::exception& Exception)
	{
		TStringConversion<TStringConvert<char, TCHAR>> Convertor(Exception.what());
		UE_LOG(LogImageWrapper, Error, TEXT("Cannot decompress EXR image: %s"), Convertor.Get());
		SetError(Convertor.Get());
		return;
	}

	// EXR channels are compressed non-interleaved.
	RawData.SetNumUninitialized((int64)BytesPerChannelPixel * ChannelCount * Width * Height);
	for (int64 OffsetNonInterleaved = 0, OffsetInterleaved = 0; OffsetInterleaved < RawData.Num(); OffsetNonInterleaved += BytesPerChannelPixel)
	{
		for (int32 c = 0; c < ChannelCount; ++c)
		{
			for (int32 b = 0; b < BytesPerChannelPixel; ++b, ++OffsetInterleaved)
			{
				RawData[OffsetInterleaved] = ChannelData[c][OffsetNonInterleaved + b];
			}
		}
	}

	Format = InFormat;
	BitDepth = InBitDepth;
}




#endif // WITH_UNREALEXR
