// Copyright Epic Games, Inc. All Rights Reserved.

#include "Texture/InterchangePCXTranslator.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InterchangeImportLog.h"
#include "InterchangeTextureNode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Texture/TextureTranslatorUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangePCXTranslator)

static bool GInterchangeEnablePCXImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnablePCXImport(
	TEXT("Interchange.FeatureFlags.Import.PCX"),
	GInterchangeEnablePCXImport,
	TEXT("Whether PCX support is enabled."),
	ECVF_Default);

//////////////////////////////////////////////////////////////////////////
// PCX helper local function
namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			// .PCX file header.
#pragma pack(push,1)

			class FPCXFileHeader
			{
			public:
				uint8	Manufacturer;		// Always 10.
				uint8	Version;			// PCX file version.
				uint8	Encoding;			// 1=run-length, 0=none.
				uint8	BitsPerPixel;		// 1,2,4, or 8.
				uint16	XMin;				// Dimensions of the image.
				uint16	YMin;				// Dimensions of the image.
				uint16	XMax;				// Dimensions of the image.
				uint16	YMax;				// Dimensions of the image.
				uint16	XDotsPerInch;		// Horizontal printer resolution.
				uint16	YDotsPerInch;		// Vertical printer resolution.
				uint8	OldColorMap[48];	// Old colormap info data.
				uint8	Reserved1;			// Must be 0.
				uint8	NumPlanes;			// Number of color planes (1, 3, 4, etc).
				uint16	BytesPerLine;		// Number of bytes per scanline.
				uint16	PaletteType;		// How to interpret palette: 1=color, 2=gray.
				uint16	HScreenSize;		// Horizontal monitor size.
				uint16	VScreenSize;		// Vertical monitor size.
				uint8	Reserved2[54];		// Must be 0.
				friend FArchive& operator<<(FArchive& Ar, FPCXFileHeader& H)
				{
					Ar << H.Manufacturer << H.Version << H.Encoding << H.BitsPerPixel;
					Ar << H.XMin << H.YMin << H.XMax << H.YMax << H.XDotsPerInch << H.YDotsPerInch;
					for (int32 i = 0; i < UE_ARRAY_COUNT(H.OldColorMap); i++)
						Ar << H.OldColorMap[i];
					Ar << H.Reserved1 << H.NumPlanes;
					Ar << H.BytesPerLine << H.PaletteType << H.HScreenSize << H.VScreenSize;
					for (int32 i = 0; i < UE_ARRAY_COUNT(H.Reserved2); i++)
						Ar << H.Reserved2[i];
					return Ar;
				}
			};

#pragma pack(pop)
		}//ns Private
	}//ns Interchange
}//ns UE

TArray<FString> UInterchangePCXTranslator::GetSupportedFormats() const
{
	if (GInterchangeEnablePCXImport || GIsAutomationTesting)
	{
		TArray<FString> Formats{ TEXT("pcx;Picture Exchange") };
		return Formats;
	}
	else
	{
		return TArray<FString>{};
	}
}

bool UInterchangePCXTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	return UE::Interchange::FTextureTranslatorUtilities::Generic2DTextureTranslate(GetSourceData(), BaseNodeContainer);
}

TOptional<UE::Interchange::FImportImage> UInterchangePCXTranslator::GetTexturePayloadData(const FString& /*PayloadKey*/, TOptional<FString>& AlternateTexturePath) const
{
	using namespace UE::Interchange;

	TArray64<uint8> SourceDataBuffer;
	if (!FTextureTranslatorUtilities::LoadSourceBuffer(*this, TEXT("PCX"), SourceDataBuffer))
	{
		return {};
	}

	const uint8* Buffer = SourceDataBuffer.GetData();
	const uint8* BufferEnd = Buffer + SourceDataBuffer.Num();

	const int64 Length = BufferEnd - Buffer;

	FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	UE::Interchange::FImportImage PayloadData;

	//
	// PCX
	//
	const UE::Interchange::Private::FPCXFileHeader* PCX = (UE::Interchange::Private::FPCXFileHeader*)Buffer;
	if (Length >= sizeof(UE::Interchange::Private::FPCXFileHeader) && PCX->Manufacturer == 10)
	{
		if (PCX->XMax < PCX->XMin ||
			PCX->YMax < PCX->YMin)
		{
			FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "CorruptedFileDim", "Failed to import PCX, invalid dims.");
			FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
			return TOptional<UE::Interchange::FImportImage>();
		}

		int32 NewU = PCX->XMax + 1 - PCX->XMin;
		int32 NewV = PCX->YMax + 1 - PCX->YMin;

		if (PCX->NumPlanes == 1 && PCX->BitsPerPixel == 8)
		{
			// Set texture properties.
			PayloadData.Init2DWithOneMip(
				NewU,
				NewV,
				TSF_BGRA8
			);
			FColor* DestPtr = (FColor*)PayloadData.RawData.GetData();
			uint64 DestSize = PayloadData.RawData.GetSize();
			FColor* DestEnd = DestPtr + DestSize;

			if (Length < 256*3)
			{
				FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
				FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
				return TOptional<UE::Interchange::FImportImage>();
			}

			// Import the palette.
			uint8* PCXPalette = (uint8*)(Buffer + Length - 256 * 3);
			TArray<FColor>	Palette;
			for (uint32 i = 0; i < 256; i++)
			{
				Palette.Add(FColor(PCXPalette[i * 3 + 0], PCXPalette[i * 3 + 1], PCXPalette[i * 3 + 2], i == 0 ? 0 : 255));
			}

			// Import it.
			if (BufferEnd - Buffer < 128)
			{
				FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
				FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
				return TOptional<UE::Interchange::FImportImage>();
			}

			Buffer += 128;
			while (DestPtr < DestEnd)
			{
				if (BufferEnd == Buffer)
				{
					FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
					FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
					return TOptional<UE::Interchange::FImportImage>();
				}
				uint8 Color = *Buffer++;

				if ((Color & 0xc0) == 0xc0)
				{
					uint32 RunLength = Color & 0x3f;

					if (BufferEnd == Buffer)
					{
						FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
						FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
						return TOptional<UE::Interchange::FImportImage>();
					}
					Color = *Buffer++;

					if (DestEnd - DestPtr < RunLength)
					{
						FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidRunLength", "Failed to import PCX, RLE length is invalid during decode");
						FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
						return TOptional<UE::Interchange::FImportImage>();
					}

					for (uint32 Index = 0; Index < RunLength; Index++)
					{
						*DestPtr++ = Palette[Color];
					}
				}
				else
				{
					*DestPtr++ = Palette[Color];
				}
			}
		}
		else if (PCX->NumPlanes == 3 && PCX->BitsPerPixel == 8)
		{
			// Set texture properties.
			PayloadData.Init2DWithOneMip(
				NewU,
				NewV,
				TSF_BGRA8
			);

			uint8* Dest = static_cast<uint8*>(PayloadData.RawData.GetData());
			uint64 DestSize = PayloadData.RawData.GetSize();
			// Doing a memset to make sure the alpha channel is set to 0xff since we only have 3 color planes.
			FMemory::Memset(Dest, 0xff, DestSize);

			// Copy upside-down scanlines.
			if (BufferEnd - Buffer < 128)
			{
				FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
				FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
				return TOptional<UE::Interchange::FImportImage>();
			}
			Buffer += 128;

			int32 CountU = FMath::Min<int32>(PCX->BytesPerLine, NewU);
			for (int32 i = 0; i < NewV; i++)
			{
				// We need to decode image one line per time building RGB image color plane by color plane.
				int32 RunLength, Overflow = 0;
				uint8 Color = 0;
				for (int32 ColorPlane = 2; ColorPlane >= 0; ColorPlane--)
				{
					for (int32 j = 0; j < CountU; j++)
					{
						if (!Overflow)
						{
							if (Buffer == BufferEnd)
							{
								FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
								FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
								return TOptional<UE::Interchange::FImportImage>();
							}
							Color = *Buffer++;
							if ((Color & 0xc0) == 0xc0)
							{
								RunLength = FMath::Min((Color & 0x3f), CountU - j);
								Overflow = (Color & 0x3f) - RunLength;
								if (Buffer == BufferEnd)
								{
									FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidFileSize", "Failed to import PCX, file size insufficient for image size.");
									FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
									return TOptional<UE::Interchange::FImportImage>();
								}
								Color = *Buffer++;
							}
							else
								RunLength = 1;
						}
						else
						{
							RunLength = FMath::Min(Overflow, CountU - j);
							Overflow = Overflow - RunLength;
						}

						// NewU is max uint16, i is max uint16, j is max uint16, runlength is max uint16.
						uint64 FarthestOffset = ((uint64)i * NewU + (j + RunLength - 1))*4 + ColorPlane;

						if (FarthestOffset >= DestSize)
						{
							FText ErrorMessage = NSLOCTEXT("InterchangePCXTranslator", "InvalidRunLength", "Failed to import PCX, RLE length is invalid during decode");
							FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
							return TOptional<UE::Interchange::FImportImage>();
						}

						//checkf(((i*NewU + RunLength) * 4 + ColorPlane) < (Texture->Source.CalcMipSize(0)),
						//	TEXT("RLE going off the end of buffer"));
						for (int32 k = j; k < j + RunLength; k++)
						{
							Dest[(i * NewU + k) * 4 + ColorPlane] = Color;
						}
						j += RunLength - 1;
					}
				}
			}
		}
		else
		{
			FText ErrorMessage = FText::Format(NSLOCTEXT("InterchangePCXTranslator", "UnsupportedFormat", "Failed to import PCX, unsupported format. ({0}{1})"), (int)PCX->NumPlanes, (int)PCX->BitsPerPixel);
			FTextureTranslatorUtilities::LogError(*this, MoveTemp(ErrorMessage));
			return TOptional<UE::Interchange::FImportImage>();
		}
	}
	else
	{
		FTextureTranslatorUtilities::LogError(*this, NSLOCTEXT("InterchangePCXTranslator", "UnsupportedVersion", "Failed to import PCX, unsupported file version."));
		return TOptional<UE::Interchange::FImportImage>();
	}

	return PayloadData;
}

