// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualizationUtilities.h"

#include "HAL/PlatformProcess.h"
#include "IO/IoHash.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/PackageResourceManager.h"

namespace UE::Virtualization::Utils
{

void PayloadIdToPath(const FIoHash& Id, FStringBuilderBase& OutPath)
{
	OutPath.Reset();
	OutPath << Id;

	TStringBuilder<10> Directory;
	Directory << OutPath.ToView().Left(2) << TEXT("/");
	Directory << OutPath.ToView().Mid(2, 2) << TEXT("/");
	Directory << OutPath.ToView().Mid(4, 2) << TEXT("/");

	OutPath.ReplaceAt(0, 6, Directory);

	OutPath << TEXT(".upayload");
}

FString PayloadIdToPath(const FIoHash& Id)
{
	TStringBuilder<52> Path;
	PayloadIdToPath(Id, Path);

	return FString(Path);
}

void GetFormattedSystemError(FStringBuilderBase& SystemErrorMessage)
{
	SystemErrorMessage.Reset();

	const uint32 SystemError = FPlatformMisc::GetLastError();
	// If we have a system error we can give a more informative error message but don't output it if the error is zero as 
	// this can lead to very confusing error messages.
	if (SystemError != 0)
	{
		TCHAR SystemErrorMsg[MAX_SPRINTF] = { 0 };
		FPlatformMisc::GetSystemErrorMessage(SystemErrorMsg, sizeof(SystemErrorMsg), SystemError);

		SystemErrorMessage.Appendf(TEXT("'%s' (%d)"), SystemErrorMsg, SystemError);
	}
	else
	{
		SystemErrorMessage << TEXT("'unknown reason' (0)");
	}
}

ETrailerFailedReason FindTrailerFailedReason(const FPackagePath& PackagePath)
{
	TUniquePtr<FArchive> Ar = IPackageResourceManager::Get().OpenReadExternalResource(EPackageExternalResource::WorkspaceDomainFile, PackagePath.GetPackageName());

	if (!Ar)
	{
		return ETrailerFailedReason::NotFound;
	}

	FPackageFileSummary Summary;
	*Ar << Summary;

	if (Ar->IsError() || Summary.Tag != PACKAGE_FILE_TAG)
	{
		return ETrailerFailedReason::InvalidSummary;
	}

	if (Summary.GetFileVersionUE() < EUnrealEngineObjectUE5Version::PAYLOAD_TOC)
	{
		return ETrailerFailedReason::OutOfDate;
	}

	return ETrailerFailedReason::Unknown;
}

bool ExpandEnvironmentVariables(FStringView InputPath, FStringBuilderBase& OutExpandedPath)
{
	while (true)
	{
		const int32 EnvVarStart = InputPath.Find(TEXT("$("));
		if (EnvVarStart == INDEX_NONE)
		{
			// If we haven't expanded anything yet we can just copy the input path
			// If we have expanded then we need to append the remainder of the path
			if (OutExpandedPath.Len() == 0)
			{
				OutExpandedPath = InputPath;
				return true;
			}
			else
			{
				OutExpandedPath << InputPath;
				return true;
			}
		}

		const int32 EnvVarEnd = InputPath.Find(TEXT(")"), EnvVarStart + 2);
		const int32 EnvVarNameLength = EnvVarEnd - (EnvVarStart + 2);

		TStringBuilder<128> EnvVarName;
		EnvVarName = InputPath.Mid(EnvVarStart + 2, EnvVarNameLength);

		FString EnvVarValue;
		if (EnvVarName.ToView() == TEXT("Temp") || EnvVarName.ToView() == TEXT("Tmp"))
		{
			// On windows the temp envvar is often in 8.3 format
			// Either we need to expose ::GetLongPathName in some way or we need to consider
			// calling it in WindowsPlatformMisc::GetEnvironmentVariable.
			// Until we decide this is a quick work around, check for the Temp envvar and if
			// it is being requested us the ::UserTempDir function which will convert 8.3 
			// format correctly.
			// This should be solved before we consider moving this utility function into core
			EnvVarValue = FPlatformProcess::UserTempDir();

			FPaths::NormalizeDirectoryName(EnvVarValue);
		}
		else
		{
			EnvVarValue = FPlatformMisc::GetEnvironmentVariable(EnvVarName.ToString());
			if (EnvVarValue.IsEmpty())
			{
				UE_LOG(LogVirtualization, Warning, TEXT("Could not find environment variable '%s' to expand"), EnvVarName.ToString());
				OutExpandedPath.Reset();
				return false;
			}
		}

		OutExpandedPath << InputPath.Mid(0, EnvVarStart);
		OutExpandedPath << EnvVarValue;

		InputPath = InputPath.Mid(EnvVarEnd + 1);
	}
}

bool IsProcessInteractive()
{
	if (FApp::IsUnattended())
	{
		return false;
	}

	if (IsRunningCommandlet())
	{
		return false;
	}

	// We used to check 'GIsRunningUnattendedScript' here as well but there
	// are a number of places in the editor enabling this global during which
	// the editor does stay interactive, such as when rendering thumbnail
	// images for the content browser.
	// Leaving this comment block here to show why we are not checking this
	// value anymore.

	if (IS_PROGRAM)
	{
		return false;
	}

	return true;
}

} // namespace UE::Virtualization::Utils
