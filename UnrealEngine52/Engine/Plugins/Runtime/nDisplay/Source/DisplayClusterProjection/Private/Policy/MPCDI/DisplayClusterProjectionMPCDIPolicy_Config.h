// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/FileHelper.h"

#include "WarpBlend/DisplayClusterWarpEnums.h"

#include "Policy/DisplayClusterProjectionPolicyBase.h"

struct FConfigParser
{
	FString  MPCDIFileName; // Single mpcdi file name

	FString  BufferId;
	FString  RegionId;

	FString  OriginType;

	// Support external pfm (warp)  and png(blend) files
	EDisplayClusterWarpProfileType  MPCDIType;

	FString  PFMFile;
	float    PFMFileScale;
	bool     bIsUnrealGameSpace = false;

	FString  AlphaFile;
	float AlphaGamma;

	FString  BetaFile;

	bool     bEnablePreview = false;

	inline bool ImplLoadConfig(IDisplayClusterViewport* InViewport, const TMap<FString, FString>& InConfigParameters)
	{
		FString MPCDITypeKey;
		if (DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::MPCDITypeKey, MPCDITypeKey))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found Argument '%s'='%s'"), DisplayClusterProjectionStrings::cfg::mpcdi::MPCDITypeKey, *MPCDITypeKey);
		}

		if (MPCDITypeKey.IsEmpty())
		{
			if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjectionMPCDI, Error, TEXT("Undefined mpcdi type key '%s'='%s'"), DisplayClusterProjectionStrings::cfg::mpcdi::MPCDITypeKey, *MPCDITypeKey);
			}
			return false;
		}

		if (MPCDITypeKey.Compare(DisplayClusterProjectionStrings::cfg::mpcdi::TypeMPCDI) == 0)
		{
			return ImplLoadMPCDIConfig(InViewport, InConfigParameters) && ImplLoadBase(InViewport, InConfigParameters);
		}

		if (MPCDITypeKey.Compare(DisplayClusterProjectionStrings::cfg::mpcdi::TypePFM) == 0)
		{
			return ImplLoadPFMConfig(InViewport, InConfigParameters) && ImplLoadBase(InViewport, InConfigParameters);
		}

		UE_LOG(LogDisplayClusterProjectionMPCDI, Error, TEXT("Unknown mpcdi type key '%s'='%s'"), DisplayClusterProjectionStrings::cfg::mpcdi::MPCDITypeKey, *MPCDITypeKey);
		return false;
	}

private:
	inline bool ImplLoadMPCDIConfig(IDisplayClusterViewport* InViewport, const TMap<FString, FString>& InConfigParameters)
	{
		// Filename
		FString LocalMPCDIFileName;
		if (DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::File, LocalMPCDIFileName))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found mpcdi file name for %s:%s - %s"), *BufferId, *RegionId, *LocalMPCDIFileName);
			MPCDIFileName = LocalMPCDIFileName;
		}

		if (MPCDIFileName.IsEmpty())
		{
			return false;
		}

		// Buffer
		if (!DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::Buffer, BufferId))
		{
			if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjectionMPCDI, Error, TEXT("Argument '%s' not found in the config file"), DisplayClusterProjectionStrings::cfg::mpcdi::Buffer);
			}

			return false;
		}

		if (BufferId.IsEmpty())
		{
			return false;
		}

		// Region
		if (!DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::Region, RegionId))
		{
			if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjectionMPCDI, Error, TEXT("Argument '%s' not found in the config file"), DisplayClusterProjectionStrings::cfg::mpcdi::Region);
			}

			return false;
		}

		if (RegionId.IsEmpty())
		{
			return false;
		}

		return true;
	}

	inline bool ImplLoadPFMConfig(IDisplayClusterViewport* InViewport, const TMap<FString, FString>& InConfigParameters)
	{
		// PFM file
		FString LocalPFMFile;
		if (DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::FilePFM, LocalPFMFile))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found Argument '%s'='%s'"), DisplayClusterProjectionStrings::cfg::mpcdi::FilePFM, *LocalPFMFile);
			PFMFile = LocalPFMFile;
		}

		if (PFMFile.IsEmpty())
		{
			return false;
		}

		// MPCDIType (optional)
		FString MPCDITypeStr;
		if (!DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::MPCDIType, MPCDITypeStr))
		{
			MPCDIType = EDisplayClusterWarpProfileType::warp_A3D;
		}
		else
		{
			MPCDIType = EDisplayClusterWarpProfileType::Invalid;

			static const TArray<FString> Profiles({ "2d","3d","a3d","sl" });
			for (int32 ProfileIndex = 0; ProfileIndex < Profiles.Num(); ++ProfileIndex)
			{
				if (!MPCDITypeStr.Compare(Profiles[ProfileIndex], ESearchCase::IgnoreCase))
				{
					MPCDIType = (EDisplayClusterWarpProfileType)ProfileIndex;
					break;
				}
			}

			if (MPCDIType == EDisplayClusterWarpProfileType::Invalid)
			{
				UE_LOG(LogDisplayClusterProjectionMPCDI, Error, TEXT("Argument '%s' has unknown value '%s'"), DisplayClusterProjectionStrings::cfg::mpcdi::MPCDIType, *MPCDITypeStr);
				return false;
			}
		}

		// Default is UE scale, cm
		PFMFileScale = 1;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::WorldScale, PFMFileScale))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found WorldScale value - %.f"), PFMFileScale);
		}

		bIsUnrealGameSpace = false;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::UseUnrealAxis, bIsUnrealGameSpace))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found bIsUnrealGameSpace value - %s"), bIsUnrealGameSpace ? TEXT("true") : TEXT("false"));
		}

		// AlphaFile file (optional)
		FString LocalAlphaFile;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::FileAlpha, LocalAlphaFile))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found external AlphaMap file - %s"), *LocalAlphaFile);
			AlphaFile = LocalAlphaFile;
		}

		AlphaGamma = 1;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::AlphaGamma, AlphaGamma))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found AlphaGamma value - %.f"), AlphaGamma);
		}

		// BetaFile file (optional)
		FString LocalBetaFile;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::FileBeta, LocalBetaFile))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found external BetaMap file - %s"), *LocalBetaFile);
			BetaFile = LocalBetaFile;
		}

		return true;
	}

	inline bool ImplLoadBase(IDisplayClusterViewport* InViewport, const TMap<FString, FString>& InConfigParameters)
	{
		// Origin node (optional)
		if (DisplayClusterHelpers::map::template ExtractValue(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::Origin, OriginType))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found origin node - %s"), *OriginType);
		}
		else
		{
			if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjectionMPCDI, Log, TEXT("No origin node found. VR root will be used as default."));
			}
		}

		bEnablePreview = false;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(InConfigParameters, DisplayClusterProjectionStrings::cfg::mpcdi::EnablePreview, bEnablePreview))
		{
			UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Found EnablePreview value - %s"), bEnablePreview ? TEXT("true") : TEXT("false"));
		}

		return true;
	}
};

