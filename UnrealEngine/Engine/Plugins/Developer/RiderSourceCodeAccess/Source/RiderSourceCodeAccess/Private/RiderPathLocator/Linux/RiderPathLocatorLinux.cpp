// Copyright Epic Games, Inc. All Rights Reserved.

#include "RiderPathLocator/RiderPathLocator.h"

#include "Internationalization/Regex.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#include "Runtime/Launch/Resources/Version.h"

#if PLATFORM_LINUX

TOptional<FInstallInfo> FRiderPathLocator::GetInstallInfoFromRiderPath(const FString& Path, FInstallInfo::EInstallType InstallType)
{
	if(!FPaths::FileExists(Path))
	{
		return {};
	}

	const FString PatternString(TEXT("(.*)(?:\\\\|/)bin"));
	const FRegexPattern Pattern(PatternString);
	FRegexMatcher RiderPathMatcher(Pattern, Path);
	if (!RiderPathMatcher.FindNext())
	{
		return {};
	}

	const FString RiderDir = RiderPathMatcher.GetCaptureGroup(1);
	const FString RiderCppPluginPath = FPaths::Combine(RiderDir, TEXT("plugins"), TEXT("rider-cpp"));
	if (!FPaths::DirectoryExists(RiderCppPluginPath))
	{
		return {};
	}

	FInstallInfo Info;
	Info.Path = Path;
	Info.InstallType = InstallType;
	const FString ProductInfoJsonPath = FPaths::Combine(RiderDir, TEXT("product-info.json"));
	if (FPaths::FileExists(ProductInfoJsonPath))
	{
		ParseProductInfoJson(Info, ProductInfoJsonPath);
	}
	if(!Info.Version.IsInitialized())
	{
		Info.Version = FPaths::GetBaseFilename(RiderDir);
		if(Info.Version.Major() >= 221)
		{
			Info.SupportUprojectState = FInstallInfo::ESupportUproject::Release;
		}
	}
	return Info;
}

static FString GetHomePath()
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 20
	TCHAR CHomePath[4096];
	FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"), CHomePath, ARRAY_COUNT(CHomePath));
	const FString FHomePath = CHomePath;
#else
	const FString FHomePath = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
#endif

	return FHomePath;
}

// Installing the standalone version of Rider does not set up path variables, and it puts Rider in /opt
static TArray<FInstallInfo> GetStandaloneInstalledRiders()
{
	TArray<FInstallInfo> Results;

	const FString OptPath = TEXT("/opt");
	const FString OptPathMask = FPaths::Combine(OptPath, TEXT("*rider*"));

	TArray<FString> FolderPaths;
	IFileManager::Get().FindFiles(FolderPaths, *OptPathMask, false, true);

	for (const FString& RiderPath : FolderPaths)
	{
		const FString FolderPath = FPaths::Combine(OptPath, RiderPath);

		TArray<FString> FullPaths;
		IFileManager::Get().FindFilesRecursive(FullPaths, *FolderPath, TEXT("rider.sh"), true, false, false);

		for (const FString& FullPath : FullPaths)
		{
			TOptional<FInstallInfo> InstallInfo = FRiderPathLocator::GetInstallInfoFromRiderPath(FullPath, FInstallInfo::EInstallType::Installed);
			if(InstallInfo.IsSet())
			{
				Results.Add(InstallInfo.GetValue());
			}
		}
	}

	return Results;
}

// Find any Rider installations on the PATH
static TArray<FInstallInfo> GetInstalledRidersFromPath()
{
	TArray<FInstallInfo> Results;

	// Search each folder on the PATH
	TArray<FString> Paths;
	FPlatformMisc::GetEnvironmentVariable(TEXT("PATH")).ParseIntoArray(Paths, FPlatformMisc::GetPathVarDelimiter());

	for (const FString& Path : Paths)
	{
		FString RiderPath = FPaths::Combine(Path, TEXT("rider.sh"));
		if (FPaths::FileExists(RiderPath))
		{
			TOptional<FInstallInfo> InstallInfo = FRiderPathLocator::GetInstallInfoFromRiderPath(RiderPath, FInstallInfo::EInstallType::Installed);
			if(InstallInfo.IsSet())
			{
				Results.Add(InstallInfo.GetValue());
			}
		}
	}

	return Results;
}

static FString GetToolboxPath()
{
	TArray<FInstallInfo> Result;
	FString LocalAppData = FPaths::Combine(GetHomePath(), TEXT(".local"), TEXT("share"));

	return FPaths::Combine(LocalAppData, TEXT("JetBrains"), TEXT("Toolbox"));
}

static TArray<FInstallInfo> GetInstalledRidersWithMdfind()
{
	int32 ReturnCode;
	FString OutResults;
	FString OutErrors;

	// avoid trying to run mdfind if it doesnt exists
	if (!FPaths::FileExists(TEXT("/usr/bin/mdfind")))
	{
		return {};
	}

	FPlatformProcess::ExecProcess(TEXT("/usr/bin/mdfind"), TEXT("\"kMDItemKind == Application\""), &ReturnCode, &OutResults, &OutErrors);
	if (ReturnCode != 0)
	{
		return {};
	}

	TArray<FString> RiderPaths;
	FString TmpString;
	while(OutResults.Split(TEXT("\n"), &TmpString, &OutResults))
	{
		if(TmpString.Contains(TEXT("Rider")))
		{
			RiderPaths.Add(TmpString);
		}
	}
	TArray<FInstallInfo> Result;
	for(const FString& RiderPath: RiderPaths)
	{
		TOptional<FInstallInfo> InstallInfo = FRiderPathLocator::GetInstallInfoFromRiderPath(RiderPath, FInstallInfo::EInstallType::Installed);
		if(InstallInfo.IsSet())
		{
			Result.Add(InstallInfo.GetValue());
		}
	}
	return Result;
}

TSet<FInstallInfo> FRiderPathLocator::CollectAllPaths()
{
	TSet<FInstallInfo> InstallInfos;
	InstallInfos.Append(GetInstalledRidersWithMdfind());
	InstallInfos.Append(GetStandaloneInstalledRiders());
	InstallInfos.Append(GetInstalledRidersFromPath());
	InstallInfos.Append(GetInstallInfosFromToolbox(GetToolboxPath(), "Rider.sh"));
	InstallInfos.Append(GetInstallInfosFromResourceFile());
	return InstallInfos;
}
#endif
