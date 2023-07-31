// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlendLoader_Texture.h"

#include "Render/DisplayClusterRenderTexture.h"

#include "DisplayClusterShadersLog.h"
#include "WarpBlend/Loader/DisplayClusterWarpBlendLoader_WarpMap.h"
#include "WarpBlend/Exporter/DisplayClusterWarpBlendExporter_WarpMap.h"

#include "Blueprints/MPCDIGeometryData.h"

#include "Misc/DisplayClusterHelpers.h"

THIRD_PARTY_INCLUDES_START
#include "mpcdiAlphaMap.h"
#include "mpcdiBetaMap.h"
#include "mpcdiBuffer.h"
#include "mpcdiDataMap.h"
#include "mpcdiDisplay.h"
#include "mpcdiDistortionMap.h"
#include "mpcdiGeometryWarpFile.h"
#include "mpcdiPfmIO.h"
#include "mpcdiPNGReadWrite.h"
#include "mpcdiProfile.h"
#include "mpcdiReader.h"
THIRD_PARTY_INCLUDES_END



IDisplayClusterRenderTexture* ImplCreateTexture(EPixelFormat InPixelFormat, uint32_t InWidth, uint32_t InHeight, const void* InTextureData, bool bInHasCPUAccess = false)
{
	check(InTextureData);
	check(InWidth > 0);
	check(InHeight > 0);

	FDisplayClusterRenderTexture* pTexture = new FDisplayClusterRenderTexture();
	pTexture->CreateTexture(InPixelFormat, InWidth, InHeight, InTextureData, bInHasCPUAccess);

	return pTexture;
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(mpcdi::DataMap* SourceDataMap)
{
	static const EPixelFormat format[4][4] =
	{
		{ PF_G8,       PF_G16,     PF_Unknown, PF_Unknown },
		{ PF_R8G8,     PF_G16R16,  PF_Unknown, PF_Unknown },
		{ PF_Unknown,  PF_Unknown, PF_Unknown, PF_Unknown },
		{ PF_R8G8B8A8, PF_Unknown, PF_Unknown, PF_Unknown },
	};

	const EPixelFormat InPixelFormat = format[SourceDataMap->GetComponentDepth() - 1][(SourceDataMap->GetBitDepth() >> 3) - 1];
	check(InPixelFormat != PF_Unknown);

	uint32_t InWidth = SourceDataMap->GetSizeX();
	uint32_t InHeight = SourceDataMap->GetSizeY();
	void* InTextureData = reinterpret_cast<void*>(SourceDataMap->GetData()->data());

	return ImplCreateTexture(InPixelFormat, InWidth, InHeight, InTextureData);
}


IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(const FString& InFileName)
{
	// Fix relative paths
	FString PNGFileName = InFileName;
	if (FPaths::IsRelative(PNGFileName))
	{
		PNGFileName = DisplayClusterHelpers::filesystem::GetFullPathForConfigResource(PNGFileName);
	}

	if (!FPaths::FileExists(PNGFileName))
	{
		UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Blend map file '%s' not found"), *PNGFileName);

		return nullptr;
	}

	const std::string FileName = TCHAR_TO_ANSI(*PNGFileName);

	mpcdi::DataMap* PngData;
	mpcdi::MPCDI_Error res = mpcdi::PNGReadWrite::Read(FileName, PngData);
	if (mpcdi::MPCDI_SUCCESS == res && PngData)
	{
		return FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(PngData);
	}

	UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Can't load blend map from file '%s'"), *PNGFileName);
	return nullptr;
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateDummyBlendMap()
{
	unsigned char White = 255;
	return ImplCreateTexture(PF_G8, 1, 1, reinterpret_cast<void*>(&White));
}

IDisplayClusterRenderTexture* ImplCreateWarpMap(const FLoadedWarpMapData& Loader)
{
	const EPixelFormat InPixelFormat = PF_A32B32G32R32F;
	return ImplCreateTexture(InPixelFormat, Loader.GetWidth(), Loader.GetHeight(), Loader.GetWarpData(), true);
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, mpcdi::GeometryWarpFile* SourceWarpMap)
{
	FLoadedWarpMapData WarpMapData;
	return FDisplayClusterWarpBlendLoader_WarpMap::Load(WarpMapData, InProfileType, SourceWarpMap) ? ImplCreateWarpMap(WarpMapData) : nullptr;
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, mpcdi::PFM* SourcePFM, float PFMScale, bool bIsUnrealGameSpace)
{
	FLoadedWarpMapData WarpMapData;
	return FDisplayClusterWarpBlendLoader_WarpMap::Load(WarpMapData, InProfileType, SourcePFM, PFMScale, bIsUnrealGameSpace) ? ImplCreateWarpMap(WarpMapData) : nullptr;
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, const TArray<FVector>& InPoints, uint32 WarpX, uint32 WarpY, float WorldScale, bool bIsUnrealGameSpace)
{
	FLoadedWarpMapData WarpMapData;
	return FDisplayClusterWarpBlendLoader_WarpMap::Load(WarpMapData, InProfileType, InPoints, WarpX, WarpY, WorldScale, bIsUnrealGameSpace) ? ImplCreateWarpMap(WarpMapData) : nullptr;
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, const struct FMPCDIGeometryImportData& Src)
{
	FLoadedWarpMapData WarpMapData;
	return FDisplayClusterWarpBlendLoader_WarpMap::Load(WarpMapData, InProfileType, Src.Vertices, Src.Width, Src.Height, 1, true) ? ImplCreateWarpMap(WarpMapData) : nullptr;
}

IDisplayClusterRenderTexture* FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(EDisplayClusterWarpProfileType ProfileType, const FString& InPFMFile, float PFMScale, bool bIsUnrealGameSpace)
{
	FString PFMFileFullPath = DisplayClusterHelpers::filesystem::GetFullPathForConfigResource(InPFMFile);

	if (!FPaths::FileExists(PFMFileFullPath))
	{
		UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("File not found: %s"), *PFMFileFullPath);
		return nullptr;
	}

	bool bResult = false;
	const std::string FileName = TCHAR_TO_ANSI(*PFMFileFullPath);
	mpcdi::PFM* PFMData;
	mpcdi::MPCDI_Error res = mpcdi::PfmIO::Read(FileName, PFMData);
	if (mpcdi::MPCDI_SUCCESS == res && PFMData)
	{
		FLoadedWarpMapData WarpMapData;
		if (FDisplayClusterWarpBlendLoader_WarpMap::Load(WarpMapData, ProfileType, PFMData, PFMScale, bIsUnrealGameSpace))
		{
			return ImplCreateWarpMap(WarpMapData);
		}
	}

	UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Can't load PFM from File %s"), *PFMFileFullPath);
	return nullptr;
}

