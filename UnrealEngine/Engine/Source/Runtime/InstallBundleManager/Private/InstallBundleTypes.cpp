// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstallBundleTypes.h"

#include "InstallBundleManagerPrivate.h"
#include "InstallBundleUtils.h"
#include "Misc/CString.h"

const TCHAR* LexToString(EInstallBundleSourceType Type)
{
	static const TCHAR* Strings[] =
	{
		TEXT("Bulk"),
		TEXT("Launcher"),
		TEXT("BuildPatchServices"),
#if WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
		TEXT("Platform"),
#endif // WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
		TEXT("GameCustom"),
		TEXT("Streaming"),
	};

	return InstallBundleUtil::TLexToString(Type, Strings);
}

void LexFromString(EInstallBundleSourceType& OutType, const TCHAR* String)
{
	OutType = EInstallBundleSourceType::Count;

	for (EInstallBundleSourceType SourceType : TEnumRange<EInstallBundleSourceType>())
	{
		const TCHAR* SourceStr = LexToString(SourceType);
		if (FCString::Stricmp(SourceStr, String) == 0)
		{
			OutType = SourceType;
			break;
		}
	}
}

const TCHAR* LexToString(EInstallBundleManagerInitResult Result)
{
	static const TCHAR* Strings[] =
	{
		TEXT("OK"),
		TEXT("BuildMetaDataNotFound"),
		TEXT("RemoteBuildMetaDataNotFound"),
		TEXT("BuildMetaDataDownloadError"),
		TEXT("BuildMetaDataParsingError"),
		TEXT("DistributionRootParseError"),
		TEXT("DistributionRootDownloadError"),
		TEXT("ManifestArchiveError"),
		TEXT("ManifestCreationError"),
		TEXT("ManifestDownloadError"),
		TEXT("BackgroundDownloadsIniDownloadError"),
		TEXT("NoInternetConnectionError"),
		TEXT("ConfigurationError"),
		TEXT("ClientPatchRequiredError"),
	};

	return InstallBundleUtil::TLexToString(Result, Strings);
}

const TCHAR* LexToString(EInstallBundleInstallState State)
{
	static const TCHAR* Strings[] =
	{
		TEXT("NotInstalled"),
		TEXT("NeedsUpdate"),
		TEXT("UpToDate"),
	};

	return InstallBundleUtil::TLexToString(State, Strings);
}

const TCHAR* LexToString(EInstallBundleResult Result)
{
	static const TCHAR* Strings[] =
	{
		TEXT("OK"),
		TEXT("FailedPrereqRequiresLatestClient"),
		TEXT("FailedPrereqRequiresLatestContent"),
		TEXT("FailedCacheReserve"),
		TEXT("InstallError"),
		TEXT("InstallerOutOfDiskSpaceError"),
		TEXT("ManifestArchiveError"),
		TEXT("ConnectivityError"),
		TEXT("UserCancelledError"),
		TEXT("InitializationError"),
		TEXT("InitializationPending"),
	};

	return InstallBundleUtil::TLexToString(Result, Strings);
}

const TCHAR* LexToString(EInstallBundleReleaseResult Result)
{
	static const TCHAR* Strings[] =
	{
		TEXT("OK"),
		TEXT("ManifestArchiveError"),
		TEXT("UserCancelledError"),
	};

	return InstallBundleUtil::TLexToString(Result, Strings);
}

const TCHAR* LexToString(EInstallBundleStatus Status)
{
	static const TCHAR* Strings[] =
	{
		TEXT("Requested"),
		TEXT("Updating"),
		TEXT("Finishing"),
		TEXT("Ready"),
	};

	return InstallBundleUtil::TLexToString(Status, Strings);
}

const TCHAR* LexToString(EInstallBundleManagerPatchCheckResult EnumVal)
{
	// These are namespaced because PartyHub expects them that way :/
	static const TCHAR* Strings[] =
	{
		TEXT("EInstallBundleManagerPatchCheckResult::NoPatchRequired"),
		TEXT("EInstallBundleManagerPatchCheckResult::ClientPatchRequired"),
		TEXT("EInstallBundleManagerPatchCheckResult::ContentPatchRequired"),
		TEXT("EInstallBundleManagerPatchCheckResult::NoLoggedInUser"),
		TEXT("EInstallBundleManagerPatchCheckResult::PatchCheckFailure"),
	};

	return InstallBundleUtil::TLexToString(EnumVal, Strings);
}

const TCHAR* LexToString(EInstallBundlePriority Priority)
{
	static const TCHAR* Strings[] =
	{
		TEXT("High"),
		TEXT("Normal"),
		TEXT("Low"),
	};

	return InstallBundleUtil::TLexToString(Priority, Strings);
}

const TCHAR* LexToString(EInstallBundleSourceUpdateBundleInfoResult Result)
{
	static const TCHAR* Strings[] =
	{
		TEXT("OK"),
		TEXT("NotInitailized"),
		TEXT("AlreadyMounted"),
		TEXT("AlreadyRequested"),
		TEXT("IllegalCacheStatus"),
	};

	return InstallBundleUtil::TLexToString(Result, Strings);
}

bool LexTryParseString(EInstallBundlePriority& OutMode, const TCHAR* InBuffer)
{
	if (FCString::Stricmp(InBuffer, TEXT("High")) == 0)
	{
		OutMode = EInstallBundlePriority::High;
		return true;
	}
	if (FCString::Stricmp(InBuffer, TEXT("Normal")) == 0)
	{
		OutMode = EInstallBundlePriority::Normal;
		return true;
	}
	if (FCString::Stricmp(InBuffer, TEXT("Low")) == 0)
	{
		OutMode = EInstallBundlePriority::Low;
		return true;
	}
	return false;
}

bool FInstallBundleCombinedInstallState::GetAllBundlesHaveState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles) const
{
	for (const TPair<FName, EInstallBundleInstallState>& Pair : IndividualBundleStates)
	{
		if (ExcludedBundles.Contains(Pair.Key))
			continue;

		if (Pair.Value != State)
			return false;
	}

	return true;
}

bool FInstallBundleCombinedInstallState::GetAnyBundleHasState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles) const
{
	for (const TPair<FName, EInstallBundleInstallState>& Pair : IndividualBundleStates)
	{
		if (ExcludedBundles.Contains(Pair.Key))
			continue;

		if (Pair.Value == State)
			return true;
	}

	return false;
}

bool FInstallBundleCombinedContentState::GetAllBundlesHaveState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles /*= TArrayView<const FName>()*/) const
{
	for (const TPair<FName, FInstallBundleContentState>& Pair : IndividualBundleStates)
	{
		if (ExcludedBundles.Contains(Pair.Key))
			continue;

		if (Pair.Value.State != State)
			return false;
	}

	return true;
}

bool FInstallBundleCombinedContentState::GetAnyBundleHasState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles /*= TArrayView<const FName>()*/) const
{
	for (const TPair<FName, FInstallBundleContentState>& Pair : IndividualBundleStates)
	{
		if (ExcludedBundles.Contains(Pair.Key))
			continue;

		if (Pair.Value.State == State)
			return true;
	}

	return false;
}
