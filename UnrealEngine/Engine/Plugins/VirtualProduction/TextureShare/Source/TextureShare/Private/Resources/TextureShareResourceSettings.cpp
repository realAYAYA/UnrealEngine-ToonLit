// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResourceSettings.h"
#include "Containers/TextureShareCoreContainers.h"

#include "RHI.h"
#include "RHIResources.h"

/**
 * Pixel<->DXGI format converter (DirectX texture exchange only supports a limited set of formats)
 */
class FTextureSharePixelFormats
{
public:
	FTextureSharePixelFormats();
	~FTextureSharePixelFormats() = default;

	static EPixelFormat GetSharedPixelFormatFromDXGI(const DXGI_FORMAT InDXGIFormat)
	{
		if (InDXGIFormat != DXGI_FORMAT_UNKNOWN)
		{
			static FTextureSharePixelFormats TextureSharePixelFormats;

			return TextureSharePixelFormats.FindPixelFormat(InDXGIFormat);
		}

		return PF_Unknown;
	}

private:
	EPixelFormat FindPixelFormat(const DXGI_FORMAT InDXGIFormat) const
	{
		if ((int32)(InDXGIFormat) >= 0 && (int32)(InDXGIFormat) < DXGIFormatsMap.Num())
		{
			return DXGIFormatsMap[(int32)(InDXGIFormat)];
		}

		return PF_Unknown;
	}

private:
	TArray<EPixelFormat> DXGIFormatsMap;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResourceSettings
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareResourceSettings::FTextureShareResourceSettings(const FTextureShareCoreResourceRequest& InResourceRequest, FRHITexture* InTexture)
{
	check(InTexture);

	// Find required texture size
	{
		// round 1
		Size = InResourceRequest.Size;

		if (Size.GetMin() < 1)
		{
			// final round
			const FIntVector  InSizeXYZ(InTexture->GetSizeXYZ());
			Size = FIntPoint(InSizeXYZ.X, InSizeXYZ.Y);
		}
	}

	// Find required pixel format:
	{
		// round 1
		Format = InResourceRequest.PixelFormat;

		if (Format == EPixelFormat::PF_Unknown)
		{
			// round 2
			Format = FTextureSharePixelFormats::GetSharedPixelFormatFromDXGI(InResourceRequest.Format);
		}

		if (Format == EPixelFormat::PF_Unknown)
		{
			// final round
			Format = InTexture->GetFormat();
		}
	}

	// Not all types of texture formats can be used for sharing.
	// Use only formats supported by shared D3D
	switch (Format)
	{
	case PF_A2B10G10R10:
	case PF_R8G8B8A8:
	case PF_A8R8G8B8:
	case PF_B8G8R8A8:
	case PF_FloatRGBA:
		// These formats are supported without conversion
		break;

	// 16-bit depth
	case PF_A16B16G16R16:
	case PF_R16G16B16A16_UNORM:
	case PF_R16G16B16A16_SNORM:
	case PF_R16G16B16A16_UINT:
	case PF_R16G16B16A16_SINT:
	case PF_G16R16:
	case PF_G16R16F:
	case PF_G16R16F_FILTER:
	case PF_G16R16_SNORM:
	case PF_R16G16_UINT:
	case PF_R16_UINT:
	case PF_R16_SINT:
	case PF_G16:
		// These formats are not supported without conversion and must be converted to 16-bit ARGB to prevent data loss.
		Format = PF_A16B16G16R16;
		break;

	// 32-bit depth
	case PF_DepthStencil:
	case PF_ShadowDepth:
	case PF_D24:
	case PF_X24_G8:
	case PF_G32R32F:
	case PF_R32G32_UINT:
	case PF_R32_UINT:
	case PF_R32_SINT:
	case PF_R32_FLOAT:
	case PF_R32G32B32_UINT:
	case PF_R32G32B32_SINT:
	case PF_R32G32B32F:
	case PF_R32G32B32A32_UINT:
	case PF_A32B32G32R32F:
		// These formats are not supported without conversion and must be converted to 32-bit ARGB to prevent data loss.
		Format = EPixelFormat::PF_A32B32G32R32F;
		break;

	default:
		// By default, any format is converted to PF_B8G8R8A8.
		// This can result in loss of data for the high-bit input format.
		// Todo: Add more conversion rules for formats that don't fit the default format type.
		Format = EPixelFormat::PF_B8G8R8A8;
		break;
	}

	// now no req for nummips
	const uint32 InNumMips = InTexture->GetNumMips();
	if (InResourceRequest.NumMips > 1 && InNumMips > 1)
	{
		NumMips = FMath::Min(InNumMips, InResourceRequest.NumMips);
	}

	// collec srgb
	bShouldUseSRGB = EnumHasAnyFlags(InTexture->GetFlags(), TexCreate_SRGB);
}

bool FTextureShareResourceSettings::Equals(const FTextureShareResourceSettings& In) const
{
	return Size == In.Size
		&& Format == In.Format
		&& NumMips == In.NumMips
		&& bShouldUseSRGB == In.bShouldUseSRGB;
}

//////////////////////////////////////////////////////////////////////////////////////////////
namespace DXGIFormatsHelper
{
	static bool GetPixelFormatFromDXGI(const DXGI_FORMAT InFormat, EPixelFormat& OutPixelFormat)
	{
		if (InFormat != DXGI_FORMAT_UNKNOWN)
		{
			for (uint32 PixelFormat = 0; PixelFormat < EPixelFormat::PF_MAX; ++PixelFormat)
			{
				if (GPixelFormats[PixelFormat].Supported
					&& GPixelFormats[PixelFormat].PlatformFormat == InFormat)
				{
					// Found
					OutPixelFormat = (EPixelFormat)PixelFormat;

					return true;
				}
			}
		}

		return false;
	};

	struct FDXGIFormatMap
	{
		DXGI_FORMAT           UnormFormat = DXGI_FORMAT_UNKNOWN;
		DXGI_FORMAT           TypelessFormat = DXGI_FORMAT_UNKNOWN;

		TArray<DXGI_FORMAT> FullyTypedFormats;

		EPixelFormat FindSharedPixelFormat() const
		{
			EPixelFormat PixelFormat;

			if (GetPixelFormatFromDXGI(UnormFormat, PixelFormat))
			{
				return PixelFormat;
			}

			if (GetPixelFormatFromDXGI(TypelessFormat, PixelFormat))
			{
				return PixelFormat;
			}

			for (const DXGI_FORMAT& FormatIt : FullyTypedFormats)
			{
				if (GetPixelFormatFromDXGI(FormatIt, PixelFormat))
				{
					return PixelFormat;
				}
			}

			return PF_Unknown;
		}
	};

	// Last supported DXGI format
	static const DXGI_FORMAT DXGI_FORMAT_MAX = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

	// Grouped formats: Unorm + typeless + fully typed
	// Format in group can be copied
	static const TArray<FDXGIFormatMap> DXGIFormatGroups = {
		{
			DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_TYPELESS,
			{
				DXGI_FORMAT_R32G32B32A32_FLOAT,
				DXGI_FORMAT_R32G32B32A32_SINT
			}
		},
		{
			DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_TYPELESS,
			{
				DXGI_FORMAT_R32G32B32_FLOAT,
				DXGI_FORMAT_R32G32B32_SINT
			}
		},
		{
			DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_TYPELESS,
			{
				DXGI_FORMAT_R16G16B16A16_FLOAT,
				DXGI_FORMAT_R16G16B16A16_UINT,
				DXGI_FORMAT_R16G16B16A16_SNORM,
				DXGI_FORMAT_R16G16B16A16_SINT
			}
		},
		{
			DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_TYPELESS,
			{
				DXGI_FORMAT_R32G32_FLOAT,
				DXGI_FORMAT_R32G32_SINT
			}
		},
		{
			DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_TYPELESS,
			{
				DXGI_FORMAT_R10G10B10A2_UINT
			}
		},
		{
			DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_TYPELESS,
			{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				DXGI_FORMAT_R8G8B8A8_UINT,
				DXGI_FORMAT_R8G8B8A8_SNORM,
				DXGI_FORMAT_R8G8B8A8_SINT
			}
		},
		{
			DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_TYPELESS,
			{
				DXGI_FORMAT_R16G16_FLOAT,
				DXGI_FORMAT_R16G16_UINT,
				DXGI_FORMAT_R16G16_SNORM,
				DXGI_FORMAT_R16G16_SINT
			}
		},
		{
			DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_TYPELESS,
			{
				DXGI_FORMAT_D32_FLOAT,
				DXGI_FORMAT_R32_FLOAT,
				DXGI_FORMAT_R32_SINT
			}
		},
		{
			DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_TYPELESS,
			{
				DXGI_FORMAT_R8G8_TYPELESS,
				DXGI_FORMAT_R8G8_UNORM,
				DXGI_FORMAT_R8G8_UINT,
				DXGI_FORMAT_R8G8_SNORM,
				DXGI_FORMAT_R8G8_SINT
			}
		},
		{
			DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_TYPELESS,
			{
				DXGI_FORMAT_R16_FLOAT,
				DXGI_FORMAT_D16_UNORM,
				DXGI_FORMAT_R16_UINT,
				DXGI_FORMAT_R16_SNORM,
				DXGI_FORMAT_R16_SINT
			}
		},
		{
			DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_TYPELESS,
			{
				DXGI_FORMAT_R8_UINT,
				DXGI_FORMAT_R8_SNORM,
				DXGI_FORMAT_R8_SINT
			}
		},
		{
			DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_UNKNOWN,
			{ }
		},
		{
			DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_TYPELESS,
			{
				DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
			}
		}
	};

	static const FDXGIFormatMap* FindDXGIFormatMap(const DXGI_FORMAT InFormat)
	{
		if (InFormat != DXGI_FORMAT_UNKNOWN)
		{
			return DXGIFormatGroups.FindByPredicate([InFormat](const FDXGIFormatMap& In)
				{
					return In.TypelessFormat == InFormat
						|| In.UnormFormat == InFormat
						|| (In.FullyTypedFormats.Find(InFormat) != INDEX_NONE);
				});
		}

		return nullptr;
	}
};
using namespace DXGIFormatsHelper;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureSharePixelFormats
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureSharePixelFormats::FTextureSharePixelFormats()
{
	DXGIFormatsMap.AddZeroed(DXGI_FORMAT_MAX);

	for (int32 DXGIFormatIndex = 0; DXGIFormatIndex < DXGI_FORMAT_MAX; ++DXGIFormatIndex)
	{
		DXGI_FORMAT DXGIFormatIt = (DXGI_FORMAT)DXGIFormatIndex;
		if (const FDXGIFormatMap* DXGIFormatMap = FindDXGIFormatMap(DXGIFormatIt))
		{
			DXGIFormatsMap[DXGIFormatIndex] = DXGIFormatMap->FindSharedPixelFormat();
		}
		else
		{
			DXGIFormatsMap[DXGIFormatIndex] = PF_Unknown;
		}
	}
};
