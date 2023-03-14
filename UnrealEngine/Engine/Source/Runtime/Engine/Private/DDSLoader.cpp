// Copyright Epic Games, Inc. All Rights Reserved.


#include "DDSLoader.h"
#include "Engine/Texture2D.h"
#include "RenderUtils.h"

FDDSLoadHelper::FDDSLoadHelper(const uint8* Buffer, uint64 Length) 
	: DDSHeader(0), DDS10Header(0)
{
	check(Buffer);

	const FDDSFileHeader* DDS = (FDDSFileHeader *)(Buffer + 4);

	uint32 AlwaysRequiredFlags = DDSF_Caps | DDSF_Height | DDSF_Width | DDSF_PixelFormat;

	if(Length >= sizeof(FDDSFileHeader) + 4 &&
		Buffer[0]=='D' && Buffer[1]=='D' && Buffer[2]=='S' && Buffer[3]==' ' &&
		DDS->dwSize == sizeof(FDDSFileHeader) &&
		DDS->ddpf.dwSize == sizeof(FDDSPixelFormatHeader) &&
		(DDS->dwFlags & AlwaysRequiredFlags) == AlwaysRequiredFlags)
	{
		DDSHeader = DDS;
	}

	// Check for dx10 dds format
	if (DDS->ddpf.dwFourCC == DDSPF_DX10)
	{
		DDS10Header = (FDDS10FileHeader *)(Buffer + 4 + sizeof(FDDSFileHeader));
	}
}

bool FDDSLoadHelper::IsValid() const
{
	return DDSHeader != 0;
}

EPixelFormat FDDSLoadHelper::ComputePixelFormat() const
{
	EPixelFormat Format = PF_Unknown;

	if(!IsValid())
	{
		return Format;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_RGB) != 0 &&
		DDSHeader->ddpf.dwRGBBitCount == 32 &&
		DDSHeader->ddpf.dwRBitMask == 0x00ff0000 &&
		DDSHeader->ddpf.dwGBitMask == 0x0000ff00 &&
		DDSHeader->ddpf.dwBBitMask == 0x000000ff)
	{
		Format = PF_B8G8R8A8;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_FourCC) != 0)
	{
		if(DDSHeader->ddpf.dwFourCC == DDSPF_DXT1)
		{
			Format = PF_DXT1;
		}
		else if(DDSHeader->ddpf.dwFourCC == DDSPF_DXT3)
		{
			Format = PF_DXT3;
		}
		else if(DDSHeader->ddpf.dwFourCC == DDSPF_DXT5)
		{
			Format = PF_DXT5;
		}
		else if(DDSHeader->ddpf.dwFourCC == MAKEFOURCC('A','T','I','2') ||
			DDSHeader->ddpf.dwFourCC == MAKEFOURCC('B','C','5','S'))
		{
			Format = PF_BC5;
		}
		else if(DDSHeader->ddpf.dwFourCC == MAKEFOURCC('B','C','4','U') ||
			DDSHeader->ddpf.dwFourCC == MAKEFOURCC('A', 'T', 'I', '1') ||
			DDSHeader->ddpf.dwFourCC == MAKEFOURCC('B','C','4','S'))
		{
			Format = PF_BC4;
		}
		else if(DDSHeader->ddpf.dwFourCC == 0x71)
		{
			Format = PF_FloatRGBA;
		}
		else if (DDSHeader->ddpf.dwFourCC == DDSPF_DX10 && DDS10Header)
		{
			switch (DDS10Header->format)
			{
			case(10): Format = PF_FloatRGBA; break; // DXGI_FORMAT_R16G16B16A16_FLOAT
			case(87): // DXGI_FORMAT_B8G8R8A8_UNORM
			case(88): // DXGI_FORMAT_B8G8R8X8_UNORM
			case(91): // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
			case(93): Format = PF_B8G8R8A8; break;// DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
			case(71): // DXGI_FORMAT_BC1_UNORM
			case(72): Format = PF_DXT1; break; // DXGI_FORMAT_BC1_UNORM_SRGB
			case(74): // DXGI_FORMAT_BC2_UNORM
			case(75): Format = PF_DXT3; break; // DXGI_FORMAT_BC2_UNORM_SRGB
			case(77): // DXGI_FORMAT_BC3_UNORM
			case(78): Format = PF_DXT5; break; // DXGI_FORMAT_BC3_UNORM_SRGB
			case(80): // DXGI_FORMAT_BC4_UNORM
			case(81): Format = PF_BC4; break; // DXGI_FORMAT_BC4_SNORM
			case(83): // DXGI_FORMAT_BC4_UNORM
			case(84): Format = PF_BC5; break; // DXGI_FORMAT_BC4_SNORM
			}
		}
	}
	return Format; 
}

ETextureSourceFormat FDDSLoadHelper::ComputeSourceFormat() const
{
	ETextureSourceFormat Format = TSF_Invalid;

	if(!IsValid())
	{
		return Format;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_RGB) != 0 &&
		DDSHeader->ddpf.dwRGBBitCount == 32 &&
		DDSHeader->ddpf.dwRBitMask == 0x00ff0000 &&
		DDSHeader->ddpf.dwGBitMask == 0x0000ff00 &&
		DDSHeader->ddpf.dwBBitMask == 0x000000ff)
	{
		Format = TSF_BGRA8;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_FourCC) != 0)
	{
		if(DDSHeader->ddpf.dwFourCC == 0x71)
		{
			Format = TSF_RGBA16F;
		}

	}

	// Check for dx10 header and extract the format
	if (DDSHeader->ddpf.dwFourCC == DDSPF_DX10 && DDS10Header)
	{
		switch (DDS10Header->format)
		{
		case(10): Format = TSF_RGBA16F; break; // DXGI_FORMAT_R16G16B16A16_FLOAT
		case(87): // DXGI_FORMAT_B8G8R8A8_UNORM
		case(88): // DXGI_FORMAT_B8G8R8X8_UNORM
		case(91): // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
		case(93): Format = TSF_BGRA8; break;// DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
		}
	}
	return Format; 
}

bool FDDSLoadHelper::IsValidCubemapTexture() const
{
	if (DDSHeader != nullptr  && DDSHeader->dwWidth == DDSHeader->dwHeight)
	{ 
		if((DDSHeader->dwCaps2 & DDSC_CubeMap) != 0 && (DDSHeader->dwCaps2 & DDSC_CubeMap_AllFaces) != 0)
		{
			return true;
		}

		if (DDS10Header != nullptr && DDS10Header->resourceType == 3 && (DDS10Header->miscFlag & 4))
		{
			return true;
		}
	}
	return false;
}

bool FDDSLoadHelper::IsValidArrayTexture() const
{
	if (DDS10Header && DDS10Header->resourceType == 3 && DDS10Header->arraySize > 1 && (DDS10Header->miscFlag & 4) == 0)
	{
		return true;
	}
	return false;
}

uint32 FDDSLoadHelper::GetSizeX() const
{
	return DDSHeader ? DDSHeader->dwWidth : 0;
}

uint32 FDDSLoadHelper::GetSizeY() const
{
	return DDSHeader ? DDSHeader->dwHeight : 0;
}

uint32 FDDSLoadHelper::GetSliceCount() const
{
	if (IsValidCubemapTexture())
	{
		return 6;
	}
	else if (IsValidArrayTexture())
	{
		return DDS10Header ? DDS10Header->arraySize : 0;
	}
	else
	{
		return 1;
	}
}

bool FDDSLoadHelper::IsValid2DTexture() const
{
	if (IsValid() && (DDSHeader->dwCaps2 & DDSC_CubeMap) == 0 && (DDSHeader->dwCaps2 & DDSC_Volume) == 0 && (DDS10Header == nullptr || (DDS10Header->resourceType == 3 && DDS10Header->arraySize == 1)))
	{
		return true;
	}

	return false;
}

uint32 FDDSLoadHelper::ComputeMipMapCount() const
{
	return (DDSHeader->dwMipMapCount > 0) ? DDSHeader->dwMipMapCount : 1;
}

const uint8* FDDSLoadHelper::GetDDSDataPointer(ECubeFace Face) const
{
	uint32 SliceSize = CalcTextureSize(GetSizeX(), GetSizeY(), ComputePixelFormat(), ComputeMipMapCount());

	const uint8* Ptr = (const uint8*)DDSHeader + sizeof(FDDSFileHeader);

	// jump over dx10 header if available
	if (DDS10Header)
	{
		Ptr += sizeof(FDDS10FileHeader);
	}

	// jump over the not requested slices / cube map sides
	Ptr += SliceSize * Face;

	return Ptr;
}


const uint8* FDDSLoadHelper::GetDDSDataPointer(const UTexture2D& Texture) const
{
	if(IsValidCubemapTexture())
	{
		FString Name = Texture.GetName();
		ECubeFace Face = GetCubeFaceFromName(Name);

		return GetDDSDataPointer(Face);
	}

	return GetDDSDataPointer();
}

int64 FDDSLoadHelper::GetDDSHeaderMaximalSize()
{
	return sizeof(FDDSFileHeader) + sizeof(FDDS10FileHeader);
}

int64 FDDSLoadHelper::GetDDSHeaderMinimalSize()
{
	return sizeof(FDDSFileHeader);
}

