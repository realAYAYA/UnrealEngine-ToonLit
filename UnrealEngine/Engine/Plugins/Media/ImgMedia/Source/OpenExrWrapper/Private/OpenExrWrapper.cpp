// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenExrWrapper.h"

#include <exception>
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

PRAGMA_DEFAULT_VISIBILITY_START
THIRD_PARTY_INCLUDES_START
	#include "Imath/ImathBox.h"
	#include "OpenEXR/ImfChannelList.h"
	#include "OpenEXR/ImfCompressionAttribute.h"
	#include "OpenEXR/ImfHeader.h"
	#include "OpenEXR/ImfIntAttribute.h"
	#include "OpenEXR/ImfOutputFile.h"
	#include "OpenEXR/ImfTileDescriptionAttribute.h"
	#include "OpenEXR/ImfRgbaFile.h"
	#include "OpenEXR/ImfStandardAttributes.h"
	#include "OpenEXR/ImfTiledInputFile.h"
	#include "OpenEXR/ImfTiledOutputFile.h"
	#include "OpenEXR/ImfTiledRgbaFile.h"
THIRD_PARTY_INCLUDES_END
PRAGMA_DEFAULT_VISIBILITY_END

DECLARE_LOG_CATEGORY_EXTERN(LogOpenEXRWrapper, Log, All);
DEFINE_LOG_CATEGORY(LogOpenEXRWrapper);


/* FOpenExr
 *****************************************************************************/

void FOpenExr::SetGlobalThreadCount(uint16 ThreadCount)
{
	Imf::setGlobalThreadCount(ThreadCount);
}


/* FRgbaInputFile
 *****************************************************************************/

FRgbaInputFile::FRgbaInputFile(const FString& FilePath)
{
	try
	{
		InputFile = new Imf::RgbaInputFile(TCHAR_TO_ANSI(*FilePath));
	}
	catch (std::exception const& Exception)
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot load EXR file: %s"), StringCast<TCHAR>(Exception.what()).Get());
		InputFile = nullptr;
	}
}


FRgbaInputFile::FRgbaInputFile(const FString& FilePath, uint16 ThreadCount)
{
	try
	{
		InputFile = new Imf::RgbaInputFile(TCHAR_TO_ANSI(*FilePath), ThreadCount);
	}
	catch (std::exception const& Exception)
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot load EXR file: %s"), StringCast<TCHAR>(Exception.what()).Get());
		InputFile = nullptr;
	}
}


FRgbaInputFile::~FRgbaInputFile()
{
	delete (Imf::RgbaInputFile*)InputFile;
}


const TCHAR* FRgbaInputFile::GetCompressionName() const
{
	auto CompressionAttribute = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::CompressionAttribute>("compression");

	if (CompressionAttribute == nullptr)
	{
		return TEXT("");
	}

	Imf::Compression Compression = CompressionAttribute->value();

	switch (Compression)
	{
	case Imf::NO_COMPRESSION:
		return TEXT("Uncompressed");

	case Imf::RLE_COMPRESSION:
		return TEXT("RLE");

	case Imf::ZIPS_COMPRESSION:
		return TEXT("ZIPS");

	case Imf::ZIP_COMPRESSION:
		return TEXT("ZIP");

	case Imf::PIZ_COMPRESSION:
		return TEXT("PIZ");

	case Imf::PXR24_COMPRESSION:
		return TEXT("PXR24");

	case Imf::B44_COMPRESSION:
		return TEXT("B44");

	case Imf::B44A_COMPRESSION:
		return TEXT("B44A");

	default:
		return TEXT("Unknown");
	}
}


FIntPoint FRgbaInputFile::GetDataWindow() const
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();

	return FIntPoint(
		Win.max.x - Win.min.x + 1,
		Win.max.y - Win.min.y + 1
	);
}


FFrameRate FRgbaInputFile::GetFrameRate(const FFrameRate& DefaultValue) const
{
	auto Attribute = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::RationalAttribute>("framesPerSecond");

	if (Attribute == nullptr)
	{
		return DefaultValue;
	}

	const Imf::Rational& Value = Attribute->value();

	return FFrameRate(Value.n, Value.d);
}

int32 FRgbaInputFile::GetNumChannels() const
{
	if (InputFile == nullptr)
	{
		return 0;
	}

	Imf::RgbaChannels Channels = ((Imf::RgbaInputFile*)InputFile)->channels();
	int32 NumChannels = 3;
	switch (Channels)
	{
	case Imf::RgbaChannels::WRITE_R:
	case Imf::RgbaChannels::WRITE_G:
	case Imf::RgbaChannels::WRITE_B:
	case Imf::RgbaChannels::WRITE_A:
	case Imf::RgbaChannels::WRITE_Y:
	case Imf::RgbaChannels::WRITE_C:
		NumChannels = 1;
		break;
	case Imf::RgbaChannels::WRITE_YC:
	case Imf::RgbaChannels::WRITE_YA:
		NumChannels = 2;
		break;
	case Imf::RgbaChannels::WRITE_RGB:
	case Imf::RgbaChannels::WRITE_YCA:
		NumChannels = 3;
		break;
	case Imf::RgbaChannels::WRITE_RGBA:
		NumChannels = 4;
		break;
	default:
		break;
	}
	return NumChannels;
}

bool FRgbaInputFile::GetTileSize(FIntPoint& OutTileSize) const
{
	const Imf::TileDescriptionAttribute* TileDescAttr = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::TileDescriptionAttribute>("tiles");
	if (TileDescAttr)
	{
		Imf::TileDescription TileDesc = TileDescAttr->value();
		OutTileSize = FIntPoint(TileDesc.xSize, TileDesc.ySize);
	}
	return TileDescAttr != nullptr;
	
}

int32 FRgbaInputFile::GetUncompressedSize() const
{
	const int32 NumChannels = GetNumChannels();
	const int32 ChannelSize = sizeof(int16);
	const FIntPoint Window = GetDataWindow();

	return (Window.X * Window.Y * NumChannels * ChannelSize);
}


bool FRgbaInputFile::IsComplete() const
{
	return ((Imf::RgbaInputFile*)InputFile)->isComplete();
}


bool FRgbaInputFile::HasInputFile() const
{
	return InputFile != nullptr;
}


void FRgbaInputFile::ReadPixels(int32 StartY, int32 EndY)
{
	try
	{
		// Since we convert everything to a coordinate system that goes from 0 to infinity when we GetDataWindow()
		// we need to convert it back to original coordinate system.
		Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();

		((Imf::RgbaInputFile*)InputFile)->readPixels(StartY + Win.min.y, EndY + Win.min.y);
	}
	catch (std::exception const& Exception)
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot read EXR file: %s (%s)"),
			ANSI_TO_TCHAR(((Imf::RgbaInputFile*)InputFile)->fileName()),
			StringCast<TCHAR>(Exception.what()).Get());
	}
}


void FRgbaInputFile::SetFrameBuffer(void* Buffer, const FIntPoint& BufferDim)
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();
	((Imf::RgbaInputFile*)InputFile)->setFrameBuffer((Imf::Rgba*)Buffer - Win.min.x - Win.min.y * BufferDim.X, 1, BufferDim.X);
}

bool FRgbaInputFile::GetIntAttribute(const FString& Name, int32& Value)
{
	bool bIsAttributeFound = false;

	if (InputFile != nullptr)
	{
		const Imf::IntAttribute* Attribute =
			((Imf::RgbaInputFile*)InputFile)->header().
			findTypedAttribute<Imf::IntAttribute>(std::string(TCHAR_TO_ANSI(*Name)));

		if (Attribute != nullptr)
		{
			Value = Attribute->value();
			bIsAttributeFound = true;
		}
	}

	return bIsAttributeFound;
}

FBaseOutputFile::FBaseOutputFile(
	const FIntPoint& DisplayWindowMin,
	const FIntPoint& DisplayWindowMax,
	const FIntPoint& DataWindowMin,
	const FIntPoint& DataWindowMax)
{
	OutputFile = nullptr;

	Imath::Box2i EXRDisplayWindow = Imath::Box2i(Imath::V2i(DisplayWindowMin.X, DisplayWindowMin.Y),
		Imath::V2i(DisplayWindowMax.X, DisplayWindowMax.Y));
	Imath::Box2i EXRDataWindow = Imath::Box2i(Imath::V2i(DataWindowMin.X, DataWindowMin.Y),
		Imath::V2i(DataWindowMax.X, DataWindowMax.Y));
	
	Header = new Imf::Header(EXRDisplayWindow, EXRDataWindow, 1, IMATH_NAMESPACE::V2f(0, 0), 1, Imf::INCREASING_Y,
		Imf::NO_COMPRESSION);
}

FBaseOutputFile::~FBaseOutputFile()
{
	if (Header != nullptr)
	{
		delete (Imf::Header*)Header;
	}
	if (OutputFile != nullptr)
	{
		delete (Imf::TiledRgbaOutputFile*)OutputFile;
	}
}

void FBaseOutputFile::AddIntAttribute(const FString& Name, int32 Value)
{
	// Make sure we don't have an output file yet.
	if (OutputFile == nullptr)
	{
		((Imf::Header*)Header)->insert(std::string(StringCast<ANSICHAR>(*Name).Get()), Imf::IntAttribute(Value));
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Attribute %s added after calling CreateOutputFile."),
			*Name);
	}
}


FTiledRgbaOutputFile::FTiledRgbaOutputFile(
	const FIntPoint& DisplayWindowMin,
	const FIntPoint& DisplayWindowMax,
	const FIntPoint& DataWindowMin,
	const FIntPoint& DataWindowMax)
	: FBaseOutputFile(DisplayWindowMin, DisplayWindowMax, DataWindowMin, DataWindowMax)
{
}

void FTiledRgbaOutputFile::CreateOutputFile(const FString& FilePath, 
	int32 TileWidth, int32 TileHeight, int32 NumChannels, bool bIsMipsEnabled)
{
	if (OutputFile == nullptr)
	{
		try
		{
			// Get channels.
			Imf::RgbaChannels Channels;
			switch (NumChannels)
			{
			case 1:
				Channels = Imf::WRITE_R;
				break;
			case 2:
				Channels = Imf::WRITE_YC;
				break;
			case 3:
				Channels = Imf::WRITE_RGB;
				break;
			case 4:
				Channels = Imf::WRITE_RGBA;
				break;
			default:
				UE_LOG(LogOpenEXRWrapper, Error, TEXT("Unsupported number of channels %d"),
					NumChannels);
				Channels = Imf::WRITE_RGBA;
				break;
			}

			// Create output file.
			OutputFile = new Imf::TiledRgbaOutputFile(StringCast<ANSICHAR>(*FilePath).Get(),
				*((Imf::Header*)Header),
				Channels,
				TileWidth, TileHeight,
				bIsMipsEnabled ? Imf::MIPMAP_LEVELS : Imf::ONE_LEVEL,
				Imf::ROUND_DOWN);
		}
		catch (std::exception const& Exception)
		{
			UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot write EXR file: %s (%s)"),
				*FilePath, StringCast<TCHAR>(Exception.what()).Get());
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("Cannot create output file as it has already been created."));
	}
}

int32 FTiledRgbaOutputFile::GetNumberOfMipLevels()
{
	if (OutputFile != nullptr)
	{
		return ((Imf::TiledRgbaOutputFile*)OutputFile)->numLevels();
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("GetNumberOfMipLevels failed: CreateOutputFile has not been called yet."));
		return 0;
	}
}

void FTiledRgbaOutputFile::SetFrameBuffer(void* Buffer, const FIntPoint& Stride)
{
	if (OutputFile != nullptr)
	{
		((Imf::TiledRgbaOutputFile*)OutputFile)->setFrameBuffer((Imf::Rgba*)Buffer, Stride.X, Stride.Y);
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("SetFrameBuffer failed: CreateOutputFile has not been called yet."));
	}
}

void FTiledRgbaOutputFile::WriteTile(int32 TileX, int32 TileY, int32 MipLevel)
{
	if (OutputFile != nullptr)
	{
		try
		{
			((Imf::TiledRgbaOutputFile*)OutputFile)->writeTile(TileX, TileY, MipLevel);
		}
		catch (std::exception const& Exception)
		{
			UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot write EXR file: %s"),
				StringCast<TCHAR>(Exception.what()).Get());
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("WriteTile failed: CreateOutputFile has not been called yet."));
	}
}

FTiledOutputFile::FTiledOutputFile(
	const FIntPoint& DisplayWindowMin,
	const FIntPoint& DisplayWindowMax,
	const FIntPoint& DataWindowMin,
	const FIntPoint& DataWindowMax,
	bool bInIsTiled)
	: FBaseOutputFile(DisplayWindowMin, DisplayWindowMax, DataWindowMin, DataWindowMax)
	, bIsTiled(bInIsTiled)
{
	FrameBuffer = new Imf::FrameBuffer;
}

FTiledOutputFile::~FTiledOutputFile()
{
	if (FrameBuffer != nullptr)
	{
		delete (Imf::FrameBuffer*)FrameBuffer;
	}
}

void FTiledOutputFile::AddChannel(const FString& Name)
{
	((Imf::Header*)Header)->channels().insert(StringCast<ANSICHAR>(*Name).Get(), Imf::Channel(Imf::HALF));
}

void FTiledOutputFile::CreateOutputFile(const FString& FilePath,
	int32 TileWidth, int32 TileHeight, bool bIsMipsEnabled, int32 NumThreads)
{
	if (OutputFile == nullptr)
	{
		try
		{
			if (bIsTiled)
			{
				((Imf::Header*)Header)->setTileDescription(Imf::TileDescription(TileWidth, TileHeight,
					bIsMipsEnabled ? Imf::MIPMAP_LEVELS : Imf::ONE_LEVEL));

				// Create output file.
				OutputFile = new Imf::TiledOutputFile(StringCast<ANSICHAR>(*FilePath).Get(),
					*((Imf::Header*)Header), NumThreads);
			}
			else
			{
				// Create output file.
				OutputFile = new Imf::OutputFile(StringCast<ANSICHAR>(*FilePath).Get(),
					*((Imf::Header*)Header), NumThreads);
			}
		}
		catch (std::exception const& Exception)
		{
			UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot write EXR file: %s (%s)"),
				*FilePath, StringCast<TCHAR>(Exception.what()).Get());
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("Cannot create output file as it has already been created."));
	}
}

void FTiledOutputFile::AddFrameBufferChannel(const FString& Name, void* Base,
	const FIntPoint& Stride)
{
	((Imf::FrameBuffer*)FrameBuffer)->insert(StringCast<ANSICHAR>(*Name).Get(),
		Imf::Slice(Imf::HALF, (char*)Base, Stride.X, Stride.Y));
}

void FTiledOutputFile::UpdateFrameBufferChannel(const FString& Name, void* Base,
	const FIntPoint& Stride)
{
	Imf::Slice* Slice = ((Imf::FrameBuffer*)FrameBuffer)->findSlice(
		StringCast<ANSICHAR>(*Name).Get());
	if (Slice != nullptr)
	{
		Slice->base = (char*)Base;
		Slice->xStride = Stride.X;
		Slice->yStride = Stride.Y;
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Could not find frame buffer channel %s."), *Name);
	}
}

void FTiledOutputFile::SetFrameBuffer()
{
	if (OutputFile != nullptr)
	{
		if (bIsTiled)
		{
			((Imf::TiledOutputFile*)OutputFile)->setFrameBuffer(*((Imf::FrameBuffer*)FrameBuffer));
		}
		else
		{
			((Imf::OutputFile*)OutputFile)->setFrameBuffer(*((Imf::FrameBuffer*)FrameBuffer));
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("Cannot set frame buffer as there is no output file."));
	}
}

void FTiledOutputFile::WriteTile(int32 TileX, int32 TileY, int32 MipLevel)
{
	if (bIsTiled)
	{
		if (OutputFile != nullptr)
		{
			try
			{
				((Imf::TiledOutputFile*)OutputFile)->writeTiles(0, TileX, 0, TileY, MipLevel);
			}
			catch (std::exception const& Exception)
			{
				UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot write EXR file: %s"),
					StringCast<TCHAR>(Exception.what()).Get());
			}
		}
		else
		{
			UE_LOG(LogOpenEXRWrapper, Error,
				TEXT("WriteTile failed: CreateOutputFile has not been called yet."));
		}
	}
	else
	{
		WriteTiles(0, 0, 0, 0, 0);
	}
}

void FTiledOutputFile::WriteTiles(int32 TileX1, int32 TileX2, int32 TileY1, int32 TileY2, int32 MipLevel)
{
	if (OutputFile != nullptr)
	{
		try
		{
			if (bIsTiled)
			{
				((Imf::TiledOutputFile*)OutputFile)->writeTiles(TileX1, TileX2, TileY1, TileY2, MipLevel);
			}
			else
			{
				Imath::Box2i DataWindow = ((Imf::Header*)Header)->dataWindow();
				((Imf::OutputFile*)OutputFile)->writePixels(DataWindow.max.y - DataWindow.min.y + 1);
			}
		}
		catch (std::exception const& Exception)
		{
			UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot write EXR file: %s"),
				StringCast<TCHAR>(Exception.what()).Get());
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("WriteTiles failed: CreateOutputFile has not been called yet."));
	}
}

int32 FTiledOutputFile::GetNumberOfMipLevels()
{
	if (OutputFile != nullptr)
	{
		if (bIsTiled)
		{
			return ((Imf::TiledOutputFile*)OutputFile)->numLevels();
		}
		else
		{
			return 1;
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("GetNumberOfMipLevels failed: CreateOutputFile has not been called yet."));
		return 0;
	}
}

int32 FTiledOutputFile::GetMipWidth(int32 MipLevel)
{
	if (OutputFile != nullptr)
	{
		if (bIsTiled)
		{
			return ((Imf::TiledOutputFile*)OutputFile)->levelWidth(MipLevel);
		}
		else
		{
			Imath::Box2i DataWindow = ((Imf::Header*)Header)->dataWindow();
			return DataWindow.size().x + 1;
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("GetMipWidth failed: CreateOutputFile has not been called yet."));
		return 0;
	}
}

int32 FTiledOutputFile::GetMipHeight(int32 MipLevel)
{
	if (OutputFile != nullptr)
	{
		if (bIsTiled)
		{
			return ((Imf::TiledOutputFile*)OutputFile)->levelHeight(MipLevel);
		}
		else
		{
			Imath::Box2i DataWindow = ((Imf::Header*)Header)->dataWindow();
			return DataWindow.size().y + 1;
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("GetMipHeight failed: CreateOutputFile has not been called yet."));
		return 0;
	}
}

int32 FTiledOutputFile::GetNumXTiles(int32 MipLevel)
{
	if (OutputFile != nullptr)
	{
		if (bIsTiled)
		{
			return ((Imf::TiledOutputFile*)OutputFile)->numXTiles(MipLevel);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("GetNumXTiles failed: CreateOutputFile has not been called yet."));
		return 0;
	}
}

int32 FTiledOutputFile::GetNumYTiles(int32 MipLevel)
{
	if (OutputFile != nullptr)
	{
		if (bIsTiled)
		{
			return ((Imf::TiledOutputFile*)OutputFile)->numYTiles(MipLevel);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		UE_LOG(LogOpenEXRWrapper, Error,
			TEXT("GetNumYTiles failed: CreateOutputFile has not been called yet."));
		return 0;
	}
}

IMPLEMENT_MODULE(FDefaultModuleImpl, OpenExrWrapper);
