// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlendLoader_MPCDI.h"

THIRD_PARTY_INCLUDES_START
#include "mpcdiProfile.h"
#include "mpcdiReader.h"
#include "mpcdiDisplay.h"
#include "mpcdiBuffer.h"
#include "mpcdiRegion.h"
#include "mpcdiAlphaMap.h"
#include "mpcdiBetaMap.h"
#include "mpcdiDistortionMap.h"
#include "mpcdiGeometryWarpFile.h"

#include "IO/mpcdiPfmIO.h"
#include "mpcdiPNGReadWrite.h"
THIRD_PARTY_INCLUDES_END

#include "DisplayClusterShadersLog.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/FileHelper.h"

#include "Stats/Stats.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#include "WarpBlend/Loader/DisplayClusterWarpBlendLoader_Texture.h"

#include "WarpBlend/DisplayClusterWarpBlend.h"
#include "WarpBlend/DisplayClusterWarpBlend_GeometryContext.h"
#include "WarpBlend/DisplayClusterWarpBlend_GeometryProxy.h"

struct FMPCDIRegionLoader
{
	EDisplayClusterWarpProfileType ProfileType;

	FMatrix RegionMatrix = FMatrix::Identity;

	float AlphaMapGammaEmbedded = 1.f;

	IDisplayClusterRenderTexture* WarpMap = nullptr;
	IDisplayClusterRenderTexture* AlphaMap = nullptr;
	IDisplayClusterRenderTexture* BetaMap = nullptr;

	TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> CreateWarpBlendInterface()
	{
		TSharedPtr<FDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend = MakeShared<FDisplayClusterWarpBlend, ESPMode::ThreadSafe>();

		WarpBlend->GeometryContext.GeometryProxy.GeometryType = EDisplayClusterWarpGeometryType::WarpMap;
		WarpBlend->GeometryContext.ProfileType = ProfileType;
		WarpBlend->GeometryContext.RegionMatrix = RegionMatrix;

		FDisplayClusterWarpBlend_GeometryProxy& Proxy = WarpBlend->GeometryContext.GeometryProxy;

		if (AlphaMap != nullptr)
		{
			Proxy.AlphaMapEmbeddedGamma = AlphaMapGammaEmbedded;

			Proxy.AlphaMapTexture = TUniquePtr<IDisplayClusterRenderTexture>(AlphaMap);
			AlphaMap = nullptr;
		}

		if (BetaMap != nullptr)
		{
			Proxy.BetaMapTexture = TUniquePtr<IDisplayClusterRenderTexture>(BetaMap);
			BetaMap = nullptr;
		}

		if (WarpMap != nullptr)
		{
			Proxy.WarpMapTexture = TUniquePtr<IDisplayClusterRenderTexture>(WarpMap);
			WarpMap = nullptr;
		}

		return WarpBlend;
	}

	void ReleaseResources()
	{
		if (WarpMap != nullptr)
		{
			delete WarpMap;
			WarpMap = nullptr;
		}
		if (AlphaMap != nullptr)
		{
			delete AlphaMap;
			AlphaMap = nullptr;
		}
		if (BetaMap != nullptr)
		{
			delete BetaMap;
			BetaMap = nullptr;
		}
	}

	bool LoadRegion(EDisplayClusterWarpProfileType InProfileType, mpcdi::Region* InRegionData)
	{
		check(InRegionData);

		ProfileType = InProfileType;

		float X = InRegionData->GetX();
		float Y = InRegionData->GetY();
		float W = InRegionData->GetXsize();
		float H = InRegionData->GetYsize();

		// Build Region matrix
		RegionMatrix = FMatrix::Identity;
		RegionMatrix.M[0][0] = W;
		RegionMatrix.M[1][1] = H;
		RegionMatrix.M[3][0] = X;
		RegionMatrix.M[3][1] = Y;

		mpcdi::AlphaMap* AlphaMapSource = InRegionData->GetFileSet()->GetAlphaMap();
		if (AlphaMapSource != nullptr)
		{
			AlphaMap = FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(AlphaMapSource);
			AlphaMapGammaEmbedded = AlphaMapSource->GetGammaEmbedded();
		}

		mpcdi::BetaMap* BetaMapSource = InRegionData->GetFileSet()->GetBetaMap();
		if (BetaMapSource != nullptr)
		{
			BetaMap = FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(BetaMapSource);
		}

		if (ProfileType != EDisplayClusterWarpProfileType::warp_SL)
		{
			mpcdi::GeometryWarpFile* WarpMapSource = InRegionData->GetFileSet()->GetGeometryWarpFile();
			if (WarpMapSource)
			{
				WarpMap = FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(ProfileType, WarpMapSource);

				if (WarpMap != nullptr)
				{
					return true;
				}
			}
		}

		// Release unused resources
		ReleaseResources();

		return false;
	}
};

EDisplayClusterWarpProfileType ImplGetProfileType(mpcdi::Profile* profile)
{
	if (profile)
	{
		switch (profile->GetProfileType())
		{
		case mpcdi::ProfileType2d: return EDisplayClusterWarpProfileType::warp_2D;
		case mpcdi::ProfileType3d: return EDisplayClusterWarpProfileType::warp_3D;
		case mpcdi::ProfileTypea3: return EDisplayClusterWarpProfileType::warp_A3D;
		case mpcdi::ProfileTypesl: return EDisplayClusterWarpProfileType::warp_SL;
		default:
			// Invalid profile type
			break;
		}
	}

	return EDisplayClusterWarpProfileType::Invalid;
};

class FMPCDIFileLoader
{
public:
	FMPCDIFileLoader()
	{ }

	~FMPCDIFileLoader()
	{ Release(); }

public:
	bool LoadMPCDIFile(const FString& InMPCDIFileName, const FString& InBufferId, const FString& InRegionId, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend)
	{
		FString MPCIDIFileFullPath = DisplayClusterHelpers::filesystem::GetFullPathForConfigResource(InMPCDIFileName);

		if (!FPaths::FileExists(MPCIDIFileFullPath))
		{
			UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("File not found: %s"), *MPCIDIFileFullPath);
			return false;
		}

		Profile = mpcdi::Profile::CreateProfile();

		{
			// Read profile data from mpcdi file using mpcdi lib:
			mpcdi::Reader* Reader = mpcdi::Reader::CreateReader();

			UE_LOG(LogDisplayClusterWarpBlend, Log, TEXT("Loading MPCDI file %s."), *MPCIDIFileFullPath);

			std::string version = Reader->GetSupportedVersions();
			FString Version = version.c_str();
			UE_LOG(LogDisplayClusterWarpBlend, Verbose, TEXT("MPCDI library version: %s"), *Version);

			mpcdi::MPCDI_Error mpcdi_err = Reader->Read(TCHAR_TO_ANSI(*MPCIDIFileFullPath), Profile);
			delete Reader;

			if (MPCDI_FAILED(mpcdi_err))
			{
				UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Error %d reading MPCDI file"), int32(mpcdi_err));
				Release();
				return false;
			}
		}

		// Read mpcdi profile type
		ProfileType = ImplGetProfileType(Profile);

		// Find desired region:
		for (mpcdi::Display::BufferIterator itBuffer = Profile->GetDisplay()->GetBufferBegin(); itBuffer != Profile->GetDisplay()->GetBufferEnd(); ++itBuffer)
		{
			mpcdi::Buffer* mpcdiBuffer = itBuffer->second;

			FString BufferId(mpcdiBuffer->GetId().c_str());
			if (InBufferId.Equals(BufferId, ESearchCase::IgnoreCase))
			{
				for (mpcdi::Buffer::RegionIterator it = mpcdiBuffer->GetRegionBegin(); it != mpcdiBuffer->GetRegionEnd(); ++it)
				{
					mpcdi::Region* mpcdiRegion = it->second;
					FString RegionId(mpcdiRegion->GetId().c_str());

					if (InRegionId.Equals(RegionId, ESearchCase::IgnoreCase))
					{
						FMPCDIRegionLoader RegionLoader;
						if (!RegionLoader.LoadRegion(ProfileType, mpcdiRegion))
						{
							UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Can't load region '%s' buffer '%s' from mpcdi file '%s'"), *InRegionId, *InBufferId, *InMPCDIFileName);
							return false;
						}

						//ok, Create and initialize warpblend interface
						OutWarpBlend = RegionLoader.CreateWarpBlendInterface();
						return true;
					}
				}
			}
		}

		UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Can't find region '%s' buffer '%s' inside mpcdi file '%s'"), *InRegionId, *InBufferId, *InMPCDIFileName);
		return false;
	}

	void Release()
	{
		if (Profile)
		{
			delete Profile;
			Profile = nullptr;
		}
	}

private:
	mpcdi::Profile* Profile = nullptr;
	EDisplayClusterWarpProfileType ProfileType = EDisplayClusterWarpProfileType::Invalid;
};

bool FDisplayClusterWarpBlendLoader_MPCDI::Load(const FDisplayClusterWarpBlendConstruct::FLoadMPCDIFile& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend)
{
	FMPCDIFileLoader Loader;
	return Loader.LoadMPCDIFile(InParameters.MPCDIFileName, InParameters.BufferId, InParameters.RegionId, OutWarpBlend);
}

bool FDisplayClusterWarpBlendLoader_MPCDI::Load(const FDisplayClusterWarpBlendConstruct::FLoadPFMFile& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend)
{
	FMPCDIRegionLoader RegionLoader;
	RegionLoader.ProfileType = InParameters.ProfileType;
	RegionLoader.AlphaMapGammaEmbedded = InParameters.AlphaMapEmbeddedAlpha;

	RegionLoader.WarpMap = FDisplayClusterWarpBlendLoader_Texture::CreateWarpMap(RegionLoader.ProfileType, InParameters.PFMFileName, InParameters.PFMScale, InParameters.bIsUnrealGameSpace);
	if (RegionLoader.WarpMap)
	{
		if (InParameters.AlphaMapFileName.IsEmpty() == false)
		{
			RegionLoader.AlphaMap = FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(InParameters.AlphaMapFileName);
		}

		if (InParameters.BetaMapFileName.IsEmpty() == false)
		{
			RegionLoader.BetaMap = FDisplayClusterWarpBlendLoader_Texture::CreateBlendMap(InParameters.BetaMapFileName);
		}

		OutWarpBlend = RegionLoader.CreateWarpBlendInterface();
		return true;
	}

	RegionLoader.ReleaseResources();

	return false;
}
