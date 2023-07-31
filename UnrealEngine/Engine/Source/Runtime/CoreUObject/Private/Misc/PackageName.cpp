// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectHash.cpp: Unreal object name hashes
=============================================================================*/

#include "Misc/PackageName.h"

#include "Algo/Find.h"
#include "Containers/StringView.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/ThreadHeartBeat.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/PackageLocalizationManager.h"
#include "IO/IoDispatcher.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackagePath.h"
#include "Misc/PackageSegment.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Stats/Stats.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/PackageResourceManager.h"

DEFINE_LOG_CATEGORY(LogPackageName);

#define LOCTEXT_NAMESPACE "PackageNames"

/** Event that is triggered when a new content path is mounted */
FPackageName::FOnContentPathMountedEvent FPackageName::OnContentPathMountedEvent;

/** Event that is triggered when a content path is dismounted */
FPackageName::FOnContentPathDismountedEvent FPackageName::OnContentPathDismountedEvent;

/** Delegate used to check whether a package exist without using the filesystem. */
FPackageName::FDoesPackageExistOverride FPackageName::DoesPackageExistOverrideDelegate;

namespace PackageNameConstants
{
	// Minimum theoretical package name length ("/A/B") is 4
	const int32 MinPackageNameLength = 4;
}

bool FPackageName::IsShortPackageName(FStringView PossiblyLongName)
{
	// Long names usually have / as first character so check from the front
	for (TCHAR Char : PossiblyLongName)
	{
		if (Char == '/')
		{
			return false;
		}
	}

	return true;
}

bool FPackageName::IsShortPackageName(const FString& PossiblyLongName)
{
	return IsShortPackageName(FStringView(PossiblyLongName));
}

bool FPackageName::IsShortPackageName(FName PossiblyLongName)
{
	// Only get "plain" part of the name. The number suffix, e.g. "_123", can't contain slashes.
	TCHAR Buffer[NAME_SIZE];
	uint32 Len = PossiblyLongName.GetPlainNameString(Buffer);
	return IsShortPackageName(FStringView(Buffer, static_cast<int32>(Len)));
}

FString FPackageName::GetShortName(const FString& LongName)
{
	// Get everything after the last slash
	int32 IndexOfLastSlash = INDEX_NONE;
	LongName.FindLastChar(TEXT('/'), IndexOfLastSlash);
	return LongName.Mid(IndexOfLastSlash + 1);
}

FString FPackageName::GetShortName(const UPackage* Package)
{
	check(Package != NULL);
	return GetShortName(Package->GetName());
}

FString FPackageName::GetShortName(const FName& LongName)
{
	return GetShortName(LongName.ToString());
}

FString FPackageName::GetShortName(const TCHAR* LongName)
{
	return GetShortName(FString(LongName));
}

FName FPackageName::GetShortFName(const FString& LongName)
{
	return GetShortFName(*LongName);
}

FName FPackageName::GetShortFName(const FName& LongName)
{
	TCHAR LongNameStr[FName::StringBufferSize];
	LongName.ToString(LongNameStr);

	if (const TCHAR* Slash = FCString::Strrchr(LongNameStr, '/'))
	{
		return FName(Slash + 1);
	}

	return LongName;
}

FName FPackageName::GetShortFName(const TCHAR* LongName)
{
	if (LongName == nullptr)
	{
		return FName();
	}

	if (const TCHAR* Slash = FCString::Strrchr(LongName, '/'))
	{
		return FName(Slash + 1);
	}

	return FName(LongName);
}

bool FPackageName::TryConvertGameRelativePackagePathToLocalPath(FStringView RelativePackagePath, FString& OutLocalPath)
{
	if (RelativePackagePath.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		// If this starts with /, this includes a root like /engine
		return FPackageName::TryConvertLongPackageNameToFilename(FString(RelativePackagePath), OutLocalPath);
	}
	else
	{
		// This is relative to /game
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		OutLocalPath = AbsoluteGameContentDir / FString(RelativePackagePath);
		return true;
	}
}


struct FPathPair
{
	// The virtual path (e.g., "/Engine/")
	const FString RootPath;

	// The physical relative path (e.g., "../../../Engine/Content/")
	const FString ContentPath;

	// The physical absolute path
	const FString AbsolutePath;

	bool operator ==(const FPathPair& Other) const
	{
		return RootPath == Other.RootPath && ContentPath == Other.ContentPath;
	}

	// Construct a path pair
	FPathPair(const FString& InRootPath, const FString& InContentPath, const FString& InAbsolutePath)
		: RootPath(InRootPath)
		, ContentPath(InContentPath)
		, AbsolutePath(InAbsolutePath)
	{
	}
};

struct FLongPackagePathsSingleton
{
	mutable FRWLock MountLock;

	FString ConfigRootPath;
	FString EngineRootPath;
	FString GameRootPath;
	FString ScriptRootPath;
	FString MemoryRootPath;
	FString TempRootPath;
	TArray<FString> MountPointRootPaths;

	FString VerseSubPath;

	FString EngineContentPath;
	FString ContentPathShort;
	FString EngineShadersPath;
	FString EngineShadersPathShort;
	FString GameContentPath;
	FString GameConfigPath;
	FString GameScriptPath;
	FString GameSavedPath;
	FString GameContentPathRebased;
	FString GameConfigPathRebased;
	FString GameScriptPathRebased;
	FString GameSavedPathRebased;

	//@TODO: Can probably consolidate these into a single array, if it weren't for EngineContentPathShort
	TArray<FPathPair> ContentRootToPath;
	TArray<FPathPair> ContentPathToRoot;

	// Cached values
	TArray<FString> ValidLongPackageRoots[2];

	// singleton
	static FLongPackagePathsSingleton& Get()
	{
		static FLongPackagePathsSingleton Singleton;
		return Singleton;
	}

	/** Initialization function to setup content paths that cannot be run until CoreUObject/PluginManager have been initialized */
	void OnCoreUObjectInitialized()
	{
		// Allow the plugin manager to mount new content paths by exposing access through a delegate.  PluginManager is 
		// a Core class, but content path functionality is added at the CoreUObject level.
		IPluginManager::Get().SetRegisterMountPointDelegate(IPluginManager::FRegisterMountPointDelegate::CreateStatic(&FPackageName::RegisterMountPoint));
		IPluginManager::Get().SetUnRegisterMountPointDelegate(IPluginManager::FRegisterMountPointDelegate::CreateStatic(&FPackageName::UnRegisterMountPoint));
	}

	const TArray<FString>& GetValidLongPackageRoots(bool bIncludeReadOnlyRoots) const
	{
		return ValidLongPackageRoots[bIncludeReadOnlyRoots ? 1 : 0];
	}

	void UpdateValidLongPackageRoots()
	{
		ValidLongPackageRoots[0].Empty();
		ValidLongPackageRoots[0].Add(EngineRootPath);
		ValidLongPackageRoots[0].Add(GameRootPath);
		ValidLongPackageRoots[0] += MountPointRootPaths;
		ValidLongPackageRoots[1] = ValidLongPackageRoots[0];
		ValidLongPackageRoots[1].Add(ConfigRootPath);
		ValidLongPackageRoots[1].Add(ScriptRootPath);
		ValidLongPackageRoots[1].Add(MemoryRootPath);
		ValidLongPackageRoots[1].Add(TempRootPath);
	}

	// Given a content path ensure it is consistent, specifically with FileManager relative paths 
	static FString ProcessContentMountPoint(const FString& ContentPath)
	{
		FString MountPath = ContentPath;

		// If a relative path is passed, convert to an absolute path 
		if (FPaths::IsRelative(MountPath))
		{
			MountPath = FPaths::ConvertRelativePathToFull(ContentPath);

			// Revert to original path if unable to convert to full path
			if (MountPath.Len() <= 1)
			{
				MountPath = ContentPath;
				UE_LOG(LogPackageName, Warning, TEXT("Unable to convert mount point relative path: %s"), *ContentPath);
			}
		}

		// Convert to a relative path using the FileManager
		return IFileManager::Get().ConvertToRelativePath(*MountPath);
	}


	// This will insert a mount point at the head of the search chain (so it can overlap an existing mount point and win)
	void InsertMountPoint(const FString& RootPath, const FString& ContentPath)
	{	
		// Make sure the content path is stored as a relative path, consistent with the other paths we have
		FString RelativeContentPath = ProcessContentMountPoint(ContentPath);

		// Make sure the path ends in a trailing path separator.  We are expecting that in the InternalFilenameToLongPackageName code.
		if( !RelativeContentPath.EndsWith( TEXT( "/" ), ESearchCase::CaseSensitive ) )
		{
			RelativeContentPath += TEXT( "/" );
		}

		TStringBuilder<256> AbsolutePath;
		FPathViews::ToAbsolutePath(RelativeContentPath, AbsolutePath);
		FPathPair Pair(RootPath, RelativeContentPath, AbsolutePath.ToString());
		
		{
			FWriteScopeLock ScopeLock(MountLock);
			ContentRootToPath.Insert(Pair, 0);
			ContentPathToRoot.Insert(Pair, 0);
			MountPointRootPaths.Add(RootPath);

			UpdateValidLongPackageRoots();
		}		

		UE_LOG(LogPackageName, Display, TEXT("FPackageName: Mount point added: '%s' mounted to '%s'"), *RelativeContentPath, *RootPath);
		
		// Let subscribers know that a new content path was mounted
		FPackageName::OnContentPathMounted().Broadcast( RootPath, RelativeContentPath);
	}

	// This will remove a previously inserted mount point
	void RemoveMountPoint(const FString& RootPath, const FString& ContentPath)
	{
		// Make sure the content path is stored as a relative path, consistent with the other paths we have
		FString RelativeContentPath = ProcessContentMountPoint(ContentPath);

		// Make sure the path ends in a trailing path separator.  We are expecting that in the InternalFilenameToLongPackageName code.
		if (!RelativeContentPath.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
		{
			RelativeContentPath += TEXT("/");
		}

		bool bFirePathDismountedDelegate = false;
		{
			FWriteScopeLock ScopeLock(MountLock);
			if ( MountPointRootPaths.Remove(RootPath) > 0 )
			{
				// Absolute path doesn't need to be computed here as we are removing mount point
				FPathPair Pair(RootPath, RelativeContentPath, FString());
				ContentRootToPath.Remove(Pair);
				ContentPathToRoot.Remove(Pair);
				MountPointRootPaths.Remove(RootPath);

				// Let subscribers know that a new content path was unmounted
				bFirePathDismountedDelegate = true;

				UpdateValidLongPackageRoots();
			}			
		}

		if (bFirePathDismountedDelegate)
		{
			FPackageName::OnContentPathDismounted().Broadcast(RootPath, RelativeContentPath);
		}
	}

	// Checks whether the specific root path is a valid mount point.
	bool MountPointExists(const FString& RootPath)
	{
		FReadScopeLock ScopeLock(MountLock);
		return GetValidLongPackageRoots(true/*bIncludeReadOnlyRoots*/).Contains(RootPath);
	}

private:
#if !UE_BUILD_SHIPPING
	const FAutoConsoleCommand DumpMountPointsCommand;
	const FAutoConsoleCommand RegisterMountPointCommand;
	const FAutoConsoleCommand UnregisterMountPointCommand;
#endif
	
	FLongPackagePathsSingleton()
#if !UE_BUILD_SHIPPING
	: DumpMountPointsCommand(
	TEXT( "PackageName.DumpMountPoints" ),
	*LOCTEXT("CommandText_DumpMountPoints", "Print registered LongPackagePath mount points").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FLongPackagePathsSingleton::ExecDumpMountPoints ) )
	, RegisterMountPointCommand(
	TEXT( "PackageName.RegisterMountPoint" ),
	*LOCTEXT("CommandText_RegisterMountPoint", "<RootPath> <ContentPath> // Register a LongPackagePath mount point").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FLongPackagePathsSingleton::ExecInsertMountPoint ) )
	, UnregisterMountPointCommand(
	TEXT( "PackageName.UnregisterMountPoint" ),
	*LOCTEXT("CommandText_UnregisterMountPoint", "<RootPath> <ContentPath> // Remove a LongPackagePath mount point").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FLongPackagePathsSingleton::ExecRemoveMountPoint ) )
#endif
	{
		SCOPED_BOOT_TIMING("FPackageName::FLongPackagePathsSingleton");

		ConfigRootPath = TEXT("/Config/");
		EngineRootPath = TEXT("/Engine/");
		GameRootPath   = TEXT("/Game/");
		ScriptRootPath = TEXT("/Script/");
		MemoryRootPath = TEXT("/Memory/");
		TempRootPath   = TEXT("/Temp/");

		VerseSubPath = TEXT("/_Verse/");

		EngineContentPath      = FPaths::EngineContentDir();
		ContentPathShort       = TEXT("../../Content/");
		EngineShadersPath      = FPaths::EngineDir() / TEXT("Shaders/");
		EngineShadersPathShort = TEXT("../../Shaders/");
		GameContentPath        = FPaths::ProjectContentDir();
		GameConfigPath         = FPaths::ProjectConfigDir();
		GameScriptPath         = FPaths::ProjectDir() / TEXT("Script/");
		GameSavedPath          = FPaths::ProjectSavedDir();

		FString RebasedGameDir = FString::Printf(TEXT("../../../%s/"), FApp::GetProjectName());

		GameContentPathRebased = RebasedGameDir / TEXT("Content/");
		GameConfigPathRebased  = RebasedGameDir / TEXT("Config/");
		GameScriptPathRebased  = RebasedGameDir / TEXT("Script/");
		GameSavedPathRebased   = RebasedGameDir / TEXT("Saved/");

		ContentPathToRoot.Empty(13);
		TStringBuilder<256> AbsolutePath;
		auto AddPathPair = [&AbsolutePath](TArray<FPathPair>& Paths, const FString& RootPath, const FString& ContentPath)
		{
			AbsolutePath.Reset();
			FPathViews::ToAbsolutePath(ContentPath, AbsolutePath);
			Paths.Emplace(RootPath, ContentPath, AbsolutePath.ToString());
		};
		
		AddPathPair(ContentPathToRoot, EngineRootPath, EngineContentPath);
		if (FPaths::IsSamePath(GameContentPath, ContentPathShort))
		{
			AddPathPair(ContentPathToRoot, GameRootPath, ContentPathShort);
		}
		else
		{
			AddPathPair(ContentPathToRoot, EngineRootPath, ContentPathShort);
		}
		AddPathPair(ContentPathToRoot, EngineRootPath, EngineShadersPath);
		AddPathPair(ContentPathToRoot, EngineRootPath, EngineShadersPathShort);
		AddPathPair(ContentPathToRoot, GameRootPath,   GameContentPath);
		AddPathPair(ContentPathToRoot, ScriptRootPath, GameScriptPath);
		AddPathPair(ContentPathToRoot, TempRootPath,   GameSavedPath);
		AddPathPair(ContentPathToRoot, GameRootPath,   GameContentPathRebased);
		AddPathPair(ContentPathToRoot, ScriptRootPath, GameScriptPathRebased);
		AddPathPair(ContentPathToRoot, TempRootPath,   GameSavedPathRebased);
		AddPathPair(ContentPathToRoot, ConfigRootPath, GameConfigPath);

		ContentRootToPath.Empty(11);
		AddPathPair(ContentRootToPath, EngineRootPath, EngineContentPath);
		AddPathPair(ContentRootToPath, EngineRootPath, EngineShadersPath);
		AddPathPair(ContentRootToPath, GameRootPath,   GameContentPath);
		AddPathPair(ContentRootToPath, ScriptRootPath, GameScriptPath);
		AddPathPair(ContentRootToPath, TempRootPath,   GameSavedPath);
		AddPathPair(ContentRootToPath, GameRootPath,   GameContentPathRebased);
		AddPathPair(ContentRootToPath, ScriptRootPath, GameScriptPathRebased);
		AddPathPair(ContentRootToPath, TempRootPath,   GameSavedPathRebased);
		AddPathPair(ContentRootToPath, ConfigRootPath, GameConfigPathRebased);

		UpdateValidLongPackageRoots();
	}

#if !UE_BUILD_SHIPPING
	void ExecDumpMountPoints(const TArray<FString>& Args)
	{
		UE_LOG(LogPackageName, Log, TEXT("Valid mount points:"));

		FReadScopeLock ScopeLock(MountLock);
		for (const auto& Pair : ContentRootToPath)
		{
			UE_LOG(LogPackageName, Log, TEXT("	'%s' -> '%s'"), *Pair.RootPath, *Pair.ContentPath);
		}

		UE_LOG(LogPackageName, Log, TEXT("Removable mount points:"));
		for (const auto& Root : MountPointRootPaths)
		{
			UE_LOG(LogPackageName, Log, TEXT("	'%s'"), *Root);
		}
		
	}

	void ExecInsertMountPoint(const TArray<FString>& Args)
	{
		if ( Args.Num() < 2 )
		{
			UE_LOG(LogPackageName, Log, TEXT("Usage: PackageName.RegisterMountPoint <RootPath> <ContentPath>"));
			UE_LOG(LogPackageName, Log, TEXT("Example ContentPath: '../../../ProjectName/Content/'"));
			UE_LOG(LogPackageName, Log, TEXT("Example RootPath: '/Game/'"));
			return;
		}

		const FString& RootPath = Args[0];
		const FString& ContentPath = Args[1];

		if (ContentPath[0] == TEXT('/'))
		{
			UE_LOG(LogPackageName, Error, TEXT("PackageName.RegisterMountPoint: Invalid ContentPath, should not start with '/'! Example: '../../../ProjectName/Content/'"));
			return;
		}

		if (RootPath[0] != TEXT('/'))
		{
			UE_LOG(LogPackageName, Error, TEXT("PackageName.RegisterMountPoint: Invalid RootPath, should start with a '/'! Example: '/Game/'"));
			return;
		}
		
		Get().InsertMountPoint(RootPath, ContentPath);
	}

	void ExecRemoveMountPoint(const TArray<FString>& Args)
	{
		if ( Args.Num() < 1 || Args.Num() > 2 )
		{
			UE_LOG(LogPackageName, Log, TEXT("Usage: PackageName.UnregisterMountPoint <Path>"));
			UE_LOG(LogPackageName, Log, TEXT("Removes either a Root or Content path if the path is unambiguous"));
			UE_LOG(LogPackageName, Log, TEXT("Usage: PackageName.UnregisterMountPoint <RootPath> <ContentPath>"));
			UE_LOG(LogPackageName, Log, TEXT("Removes a specific Root path to Content path mapping"));
			UE_LOG(LogPackageName, Log, TEXT("Example ContentPath: '../../../ProjectName/Content/'"));
			UE_LOG(LogPackageName, Log, TEXT("Example RootPath: '/Game/'"));
			return;
		}

		if (Args.Num() == 1)
		{
			const FString& Path = Args[0];
			const bool IsRootPath = Path[0] == TEXT('/');

			FString RootPath;
			FString ContentPath;
			
			if (IsRootPath)
			{
				FReadScopeLock ScopeLock(MountLock);
				
				RootPath = Path;
				auto Elements = Get().ContentRootToPath.FilterByPredicate([&RootPath](const FPathPair& elem){ return elem.RootPath.Equals(RootPath); });
				if (Elements.Num() == 0)
				{
					UE_LOG(LogPackageName, Error, TEXT("PackageName.UnregisterMountPoint: Root path '%s' is not mounted!"), *RootPath);
					return;
				}

				if (Elements.Num() > 1)
				{
					UE_LOG(LogPackageName, Error, TEXT("PackageName.UnregisterMountPoint: Root path '%s' is mounted to multiple content paths, specify content path to unmount explicitly!"), *RootPath);
					for (const FPathPair& elem : Elements)
					{
						UE_LOG(LogPackageName, Error, TEXT("- %s"), *elem.ContentPath);
					}
					return;
				}

				ContentPath = Elements[0].ContentPath;
			}
			else
			{
				FReadScopeLock ScopeLock(MountLock);
			
				ContentPath = Path;
				auto Elements = Get().ContentPathToRoot.FilterByPredicate([&ContentPath](const FPathPair& elem){ return elem.ContentPath.Equals(ContentPath); });
				if (Elements.Num() == 0)
				{
					UE_LOG(LogPackageName, Error, TEXT("PackageName.UnregisterMountPoint: Content path '%s' is not mounted!"), *ContentPath);
					return;
				}

				if (Elements.Num() > 1)
				{
					UE_LOG(LogPackageName, Error, TEXT("PackageName.UnregisterMountPoint: Content path '%s' is mounted to multiple root paths, specify root path to unmount explicitly!"), *ContentPath);
					for (const FPathPair& elem : Elements)
					{
						UE_LOG(LogPackageName, Error, TEXT("- %s"), *elem.RootPath);
					}
					return;
				}

				RootPath = Elements[0].RootPath;
			}
			
			Get().RemoveMountPoint(RootPath, ContentPath);
		}
		else
		{
			const FString& RootPath = Args[0];
			const FString& ContentPath = Args[1];
			if (ContentPath[0] == TEXT('/'))
			{
				UE_LOG(LogPackageName, Error, TEXT("PackageName.UnregisterMountPoint: Invalid ContentPath, should not start with '/'! Example: '../../../ProjectName/Content/'"));
				return;
			}

			if (RootPath[0] != TEXT('/'))
			{
				UE_LOG(LogPackageName, Error, TEXT("PackageName.UnregisterMountPoint: Invalid RootPath, should start with '/'! Example: '/Game/'"));
				return;
			}

			Get().RemoveMountPoint(RootPath, ContentPath);
		}
	}
#endif
};

void FPackageName::InternalFilenameToLongPackageName(FStringView InFilename, FStringBuilderBase& OutPackageName)
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	FString Filename(InFilename);
	FPaths::NormalizeFilename(Filename);

	// Convert to relative path if it's not already a long package name
	bool bIsValidLongPackageName = false;
	{
		FReadScopeLock ScopeLock(Paths.MountLock);
		for (const auto& Pair : Paths.ContentRootToPath)
		{
			if (FPathViews::IsParentPathOf(Pair.RootPath, Filename))
			{
				bIsValidLongPackageName = true;
				break;
			}
		}
	}

	FStringView Result;
	if (!bIsValidLongPackageName)
	{
		Filename = IFileManager::Get().ConvertToRelativePath(*Filename);
		if (InFilename.Len() > 0 && InFilename[InFilename.Len() - 1] == '/')
		{
			// If InFilename ends in / but converted doesn't, add the / back
			bool bEndsInSlash = Filename.Len() > 0 && Filename[Filename.Len() - 1] == '/';

			if (!bEndsInSlash)
			{
				Filename += TEXT("/");
			}
		}
	}
	
	FStringView SplitOutPath;
	FStringView SplitOutName;
	FStringView SplitOutExtension;
	FPathViews::Split(Filename, SplitOutPath, SplitOutName, SplitOutExtension);

	// Do not strip the extension from paths with an empty pathname; if we do the caller can't tell the difference
	// between /Path/.ext and /Path/
	if (!SplitOutName.IsEmpty() || SplitOutExtension.IsEmpty())
	{
		Result = FStringView(SplitOutPath.GetData(), UE_PTRDIFF_TO_INT32(SplitOutName.GetData() + SplitOutName.Len() - SplitOutPath.GetData()));
	}

	if (bIsValidLongPackageName && Result.Len() != Filename.Len())
	{
		UE_LOG(LogPackageName, Warning, TEXT("TryConvertFilenameToLongPackageName was passed an ObjectPath (%.*s) rather than a PackageName or FilePath; it will be converted to the PackageName. "
			"Accepting ObjectPaths is deprecated behavior and will be removed in a future release; TryConvertFilenameToLongPackageName will fail on ObjectPaths."), InFilename.Len(), InFilename.GetData());
	}

	{
		FReadScopeLock ScopeLock(Paths.MountLock);
		for (const auto& Pair : Paths.ContentPathToRoot)
		{
			FStringView RelPath;
			if (FPathViews::TryMakeChildPathRelativeTo(Result, Pair.ContentPath, RelPath))
			{
				OutPackageName << Pair.RootPath << RelPath;
				return;
			}
		}
	}

	// if we get here, we haven't converted to a package name, and it may be because the path was absolute. in which case we should 
	// check the absolute representation in ContentPathToRoot
	if (!bIsValidLongPackageName)
	{
		// reset to the incoming string
		Filename = InFilename;
		if (!FPaths::IsRelative(Filename))
		{
			FPaths::NormalizeFilename(Filename);
			Result = FPathViews::GetBaseFilenameWithPath(Filename);
			{
				FReadScopeLock ScopeLock(Paths.MountLock);
				for (const auto& Pair : Paths.ContentPathToRoot)
				{
					FStringView RelPath;
					// Compare against AbsolutePath, but also try ContentPath in case ContentPath is absolute
					if (FPathViews::TryMakeChildPathRelativeTo(Result, Pair.AbsolutePath, RelPath))
					{
						OutPackageName << Pair.RootPath << RelPath;
						return;
					}
					if (FPathViews::TryMakeChildPathRelativeTo(Result, Pair.ContentPath, RelPath))
					{
						OutPackageName << Pair.RootPath << RelPath;
						return;
					}
				}
			}
		}
	}

	OutPackageName << Result;
}

bool FPackageName::TryConvertFilenameToLongPackageName(const FString& InFilename, FString& OutPackageName, FString* OutFailureReason)
{
	TStringBuilder<256> LongPackageNameBuilder;
	InternalFilenameToLongPackageName(InFilename, LongPackageNameBuilder);
	FStringView LongPackageName = LongPackageNameBuilder.ToString();

	if (LongPackageName.IsEmpty())
	{
		if (OutFailureReason)
		{
			FStringView FilenameWithoutExtension = FPathViews::GetBaseFilenameWithPath(InFilename);
			*OutFailureReason = FString::Printf(TEXT("FilenameToLongPackageName failed to convert '%s'. The Result would be indistinguishable from using '%.*s' as the InFilename."), *InFilename, FilenameWithoutExtension.Len(), FilenameWithoutExtension.GetData());
		}
		return false;
	}

	// we don't support loading packages from outside of well defined places
	int32 CharacterIndex;
	const bool bContainsDot = LongPackageName.FindChar(TEXT('.'), CharacterIndex);
	const bool bContainsBackslash = LongPackageName.FindChar(TEXT('\\'), CharacterIndex);
	const bool bContainsColon = LongPackageName.FindChar(TEXT(':'), CharacterIndex);

	if (!(bContainsDot || bContainsBackslash || bContainsColon))
	{
		OutPackageName = LongPackageName;
		return true;
	}

	// if the package name resolution failed and a relative path was provided, convert to an absolute path
	// as content may be mounted in a different relative path to the one given
	if (FPaths::IsRelative(InFilename))
	{
		FString AbsPath = FPaths::ConvertRelativePathToFull(InFilename);
		if (!FPaths::IsRelative(AbsPath) && AbsPath.Len() > 1)
		{
			if (TryConvertFilenameToLongPackageName(AbsPath, OutPackageName, nullptr))
			{
				return true;
			}
		}
	}

	if (OutFailureReason != nullptr)
	{
		FString InvalidChars;
		if (bContainsDot)
		{
			InvalidChars += TEXT(".");
		}
		if (bContainsBackslash)
		{
			InvalidChars += TEXT("\\");
		}
		if (bContainsColon)
		{
			InvalidChars += TEXT(":");
		}
		*OutFailureReason = FString::Printf(TEXT("FilenameToLongPackageName failed to convert '%s'. Attempt result was '%.*s', but the path contains illegal characters '%s'"), *InFilename, LongPackageName.Len(), LongPackageName.GetData(), *InvalidChars);
	}

	return false;
}

FString FPackageName::FilenameToLongPackageName(const FString& InFilename)
{
	FString FailureReason;
	FString Result;
	if (!TryConvertFilenameToLongPackageName(InFilename, Result, &FailureReason))
	{
		TStringBuilder<128> ContentRoots;
		{
			const auto& Paths = FLongPackagePathsSingleton::Get();
			FReadScopeLock ScopeLock(Paths.MountLock);
			for (const FPathPair& Pair : Paths.ContentPathToRoot)
			{
				ContentRoots << TEXT("\n\t\t") << Pair.ContentPath;
				ContentRoots << TEXT("\n\t\t\t") << Pair.AbsolutePath;
			}
		}

		UE_LOG(LogPackageName, Display, TEXT("FilenameToLongPackageName failed, we will issue a fatal log. Diagnostics:")
			TEXT("\n\tInFilename=%s")
			TEXT("\n\tConvertToRelativePath=%s")
			TEXT("\n\tConvertRelativePathToFull=%s")
			TEXT("\n\tRootDir=%s")
			TEXT("\n\tBaseDir=%s")
			TEXT("\n\tContentRoots=%s"),
			*InFilename, *IFileManager::Get().ConvertToRelativePath(*InFilename),
			*FPaths::ConvertRelativePathToFull(InFilename), FPlatformMisc::RootDir(), FPlatformProcess::BaseDir(),
			ContentRoots.ToString()
		);
		UE_LOG(LogPackageName, Fatal, TEXT("%s"), *FailureReason);
	}
	return Result;
}

bool FPackageName::TryConvertLongPackageNameToFilename(const FString& InLongPackageName, FString& OutFilename, const FString& InExtension)
{
	const auto& Paths = FLongPackagePathsSingleton::Get();
	FReadScopeLock ScopeLock(Paths.MountLock);
	for (const auto& Pair : Paths.ContentRootToPath)
	{
		FStringView RelPath;
		if (FPathViews::TryMakeChildPathRelativeTo(InLongPackageName, Pair.RootPath, RelPath))
		{
			OutFilename = Pair.ContentPath + RelPath + InExtension;
			return true;
		}
	}

	// This is not a long package name or the root folder is not handled in the above cases
	return false;
}

FString FPackageName::LongPackageNameToFilename(const FString& InLongPackageName, const FString& InExtension)
{
	FString Result;
	if (!TryConvertLongPackageNameToFilename(InLongPackageName, Result, InExtension))
	{
		UE_LOG(LogPackageName, Fatal,TEXT("LongPackageNameToFilename failed to convert '%s'. Path does not map to any roots."), *InLongPackageName);
	}
	return Result;
}

bool FPackageName::TryConvertToMountedPath(FStringView InFilePathOrPackageName, FString* OutLocalPathNoExtension,
	FString* OutPackageName, FString* OutObjectName, FString* OutSubObjectName, FString* OutExtension,
	EFlexNameType* OutFlexNameType, EErrorCode* OutFailureReason)
{
	TStringBuilder<256> MountPointPackageName;
	TStringBuilder<256> MountPointFilePath;
	TStringBuilder<256> PackageNameRelPath;
	TStringBuilder<128> ObjectPath;
	TStringBuilder<16> CustomExtension;
	EPackageExtension ExtensionType;

	bool bResult = TryConvertToMountedPathComponents(InFilePathOrPackageName, MountPointPackageName,
		MountPointFilePath, PackageNameRelPath, ObjectPath, ExtensionType, CustomExtension, OutFlexNameType,
		OutFailureReason);
	if (!bResult)
	{
		if (OutLocalPathNoExtension) OutLocalPathNoExtension->Reset();
		if (OutPackageName) OutPackageName->Reset();
		if (OutObjectName) OutObjectName->Reset();
		if (OutSubObjectName) OutSubObjectName->Reset();
		if (OutExtension) OutExtension->Reset();
		if (OutFlexNameType) *OutFlexNameType = EFlexNameType::Invalid;
		return false;
	}

	if (OutLocalPathNoExtension) *OutLocalPathNoExtension = FString(MountPointFilePath) + PackageNameRelPath;
	if (OutPackageName) *OutPackageName = FString(MountPointPackageName) + PackageNameRelPath;
	if (OutObjectName || OutSubObjectName)
	{
		FStringView ObjectPathView(ObjectPath);
		int32 SubObjectStart;
		if (ObjectPathView.FindChar(SUBOBJECT_DELIMITER_CHAR, SubObjectStart))
		{
			if (OutObjectName) *OutObjectName = ObjectPathView.Left(SubObjectStart);
			if (OutSubObjectName) *OutSubObjectName = ObjectPathView.RightChop(SubObjectStart + 1);
		}
		else
		{
			if (OutObjectName) *OutObjectName = ObjectPathView;
			if (OutSubObjectName) OutSubObjectName->Reset();
		}
	}
	if (OutExtension)
	{
		*OutExtension = ExtensionType == EPackageExtension::Custom ? CustomExtension.ToString() : LexToString(ExtensionType);
	}
	return true;
}
FString FPackageName::GetLongPackagePath(const FString& InLongPackageName)
{
	int32 IndexOfLastSlash = INDEX_NONE;
	if (InLongPackageName.FindLastChar(TEXT('/'), IndexOfLastSlash))
	{
		return InLongPackageName.Left(IndexOfLastSlash);
	}
	else
	{
		return InLongPackageName;
	}
}

bool FPackageName::SplitLongPackageName(const FString& InLongPackageName, FString& OutPackageRoot, FString& OutPackagePath, FString& OutPackageName, const bool bStripRootLeadingSlash)
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();

	const bool bIncludeReadOnlyRoots = true;

	FReadScopeLock ScopeLock(Paths.MountLock);
	const TArray<FString>& ValidRoots = Paths.GetValidLongPackageRoots(bIncludeReadOnlyRoots);

	// Check to see whether our package came from a valid root
	OutPackageRoot.Empty();
	FStringView PackageRelPath;
	for(auto RootIt = ValidRoots.CreateConstIterator(); RootIt; ++RootIt)
	{
		const FString& PackageRoot = *RootIt;
		if (FPathViews::TryMakeChildPathRelativeTo(InLongPackageName, PackageRoot, PackageRelPath))
		{
			OutPackageRoot = PackageRoot / "";
			break;
		}
	}

	if(OutPackageRoot.IsEmpty() || InLongPackageName.Len() <= OutPackageRoot.Len())
	{
		// Path is not part of a valid root, or the path given is too short to continue; splitting failed
		return false;
	}

	// Use the standard path functions to get the rest
	FString PackageRelPathStr(PackageRelPath);
	OutPackagePath = FPaths::GetPath(PackageRelPathStr) / "";
	OutPackageName = FPaths::GetCleanFilename(PackageRelPathStr);

	if(bStripRootLeadingSlash && OutPackageRoot.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		OutPackageRoot.RemoveAt(0);
	}

	return true;
}

void FPackageName::SplitFullObjectPath(const FString& InFullObjectPath, FString& OutClassName,
	FString& OutPackageName, FString& OutObjectName, FString& OutSubObjectName, bool bDetectClassName)
{
	FStringView ClassName;
	FStringView PackageName;
	FStringView ObjectName;
	FStringView SubObjectName;
	SplitFullObjectPath(InFullObjectPath, ClassName, PackageName, ObjectName, SubObjectName, bDetectClassName);
	OutClassName = ClassName;
	OutPackageName = PackageName;
	OutObjectName = ObjectName;
	OutSubObjectName = SubObjectName;
}

void FPackageName::SplitFullObjectPath(FStringView InFullObjectPath, FStringView& OutClassName,
	FStringView& OutPackageName, FStringView& OutObjectName, FStringView& OutSubObjectName, bool bDetectClassName)
{
	FStringView Remaining = InFullObjectPath.TrimStartAndEnd();

	auto ExtractBeforeDelim = [&Remaining](TCHAR Delim, FStringView& OutStringView)
	{
		int32 DelimIndex;
		if (Remaining.FindChar(Delim, DelimIndex))
		{
			OutStringView = Remaining.Left(DelimIndex);
			Remaining.RightChopInline(DelimIndex + 1);
			return true;
		}
		else
		{
			OutStringView.Reset();
			return false;
		}
	};

	// If we are handling class names, split on the first space. If we are not, or there is no space,
	// then ClassName is empty and the remaining string is PackageName.ObjectName:SubObjectName
	if (bDetectClassName)
	{
		ExtractBeforeDelim(TEXT(' '), OutClassName);
	}
	else
	{
		OutClassName.Reset();
	}
	if (ExtractBeforeDelim(TEXT('.'), OutPackageName))
	{
		if (ExtractBeforeDelim(TEXT(':'), OutObjectName))
		{
			OutSubObjectName = Remaining;
		}
		else
		{
			// If no :, then the remaining string is ObjectName
			OutObjectName = Remaining;
			OutSubObjectName = FStringView();
		}
	}
	else 
	{
		// If no '.', then the remaining string is PackageName
		OutPackageName = Remaining;
		OutObjectName = FStringView();
		OutSubObjectName = FStringView();
	}
}

FString FPackageName::GetLongPackageAssetName(const FString& InLongPackageName)
{
	return GetShortName(InLongPackageName);
}

bool FPackageName::DoesPackageNameContainInvalidCharacters(FStringView InLongPackageName, FText* OutReason)
{
	EErrorCode Reason;
	if (DoesPackageNameContainInvalidCharacters(InLongPackageName, &Reason))
	{
		if (OutReason) *OutReason = FormatErrorAsText(InLongPackageName, Reason);
		return true;
	}
	return false;
}

bool FPackageName::DoesPackageNameContainInvalidCharacters(FStringView InLongPackageName, EErrorCode* OutReason /*= nullptr */)
{
	// See if the name contains invalid characters.
	TStringBuilder<32> MatchedInvalidChars;
	for (const TCHAR* InvalidCharacters = INVALID_LONGPACKAGE_CHARACTERS; *InvalidCharacters; ++InvalidCharacters)
	{
		int32 OutIndex;
		if (InLongPackageName.FindChar(*InvalidCharacters, OutIndex))
		{
			MatchedInvalidChars += *InvalidCharacters;
		}
	}
	if (MatchedInvalidChars.Len())
	{
		if (OutReason) *OutReason = EErrorCode::PackageNameContainsInvalidCharacters;
		return true;
	}
	if (OutReason) *OutReason = EErrorCode::PackageNameUnknown;
	return false;
}

bool FPackageName::IsValidTextForLongPackageName(FStringView InLongPackageName, FText* OutReason)
{
	EErrorCode Reason;
	if (!IsValidTextForLongPackageName(InLongPackageName, &Reason))
	{
		if (OutReason) *OutReason = FormatErrorAsText(InLongPackageName, Reason);
		return false;
	}
	return true;
}

bool FPackageName::IsValidTextForLongPackageName(FStringView InLongPackageName, EErrorCode* OutReason /*= nullptr */)
{
	// All package names must contain a leading slash, root, slash and name, at minimum theoretical length ("/A/B") is 4
	if (InLongPackageName.Len() < PackageNameConstants::MinPackageNameLength)
	{
		if (OutReason) *OutReason = EErrorCode::LongPackageNames_PathTooShort;
		return false;
	}
	// Package names start with a leading slash.
	if (InLongPackageName[0] != '/')
	{
		if (OutReason) *OutReason = EErrorCode::LongPackageNames_PathWithNoStartingSlash;
		return false;
	}
	// Package names do not end with a trailing slash.
	if (InLongPackageName[InLongPackageName.Len() - 1] == '/')
	{
		if (OutReason) *OutReason = EErrorCode::LongPackageNames_PathWithTrailingSlash;
		return false;
	}
	if (InLongPackageName.Contains(TEXT("//")))
	{
		if (OutReason) *OutReason = EErrorCode::LongPackageNames_PathWithDoubleSlash;
		return false;
	}
	// Check for invalid characters
	if (DoesPackageNameContainInvalidCharacters(InLongPackageName, OutReason))
	{
		return false;
	}
	if (OutReason) *OutReason = EErrorCode::PackageNameUnknown;
	return true;
}

bool FPackageName::IsValidLongPackageName(FStringView InLongPackageName, bool bIncludeReadOnlyRoots, FText* OutReason)
{
	EErrorCode Reason;
	if (!IsValidLongPackageName(InLongPackageName, bIncludeReadOnlyRoots, &Reason))
	{
		if (OutReason)
		{
			if (Reason == EErrorCode::PackageNamePathNotMounted)
			{
				const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();

				FReadScopeLock ScopeLock(Paths.MountLock);
				const TArray<FString>& ValidRoots = Paths.GetValidLongPackageRoots(bIncludeReadOnlyRoots);
				if (ValidRoots.Num() == 0)
				{
					*OutReason = NSLOCTEXT("Core", "LongPackageNames_NoValidRoots", "No valid roots exist!");
				}
				else
				{
					FString ValidRootsString = TEXT("");
					if (ValidRoots.Num() == 1)
					{
						ValidRootsString = FString::Printf(TEXT("'%s'"), *ValidRoots[0]);
					}
					else
					{
						for (int32 RootIdx = 0; RootIdx < ValidRoots.Num(); ++RootIdx)
						{
							if (RootIdx < ValidRoots.Num() - 1)
							{
								ValidRootsString += FString::Printf(TEXT("'%s', "), *ValidRoots[RootIdx]);
							}
							else
							{
								ValidRootsString += FString::Printf(TEXT("or '%s'"), *ValidRoots[RootIdx]);
							}
						}
					}
					*OutReason = FText::Format( NSLOCTEXT("Core", "LongPackageNames_InvalidRoot", "Path does not start with a valid root. Path must begin with: {0}"), FText::FromString( ValidRootsString ) );
				}
			}
			else
			{
				*OutReason = FormatErrorAsText(InLongPackageName, Reason);
			}
		}
		return false;
	}
	return true;
}

bool FPackageName::IsValidLongPackageName(FStringView InLongPackageName, bool bIncludeReadOnlyRoots /*= false*/, EErrorCode* OutReason /*= nullptr */)
{
	if (!IsValidTextForLongPackageName(InLongPackageName, OutReason))
	{
		return false;
	}

	// Check valid roots
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	FReadScopeLock ScopeLock(Paths.MountLock);
	const TArray<FString>& ValidRoots = Paths.GetValidLongPackageRoots(bIncludeReadOnlyRoots);
	bool bValidRoot = false;
	for (int32 RootIdx = 0; RootIdx < ValidRoots.Num(); ++RootIdx)
	{
		const FString& Root = ValidRoots[RootIdx];
		if (FPathViews::IsParentPathOf(Root, InLongPackageName))
		{
			if (OutReason) *OutReason = EErrorCode::PackageNameUnknown;
			return true;
		}
	}
	if (OutReason) *OutReason = EErrorCode::PackageNamePathNotMounted;
	return false;
}

bool FPackageName::IsValidObjectPath(const FString& InObjectPath, FText* OutReason)
{
	FString PackageName;
	FString RemainingObjectPath;

	// Check for package delimiter
	int32 ObjectDelimiterIdx;
	if (InObjectPath.FindChar(TEXT('.'), ObjectDelimiterIdx))
	{
		if (ObjectDelimiterIdx == InObjectPath.Len() - 1)
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("Core", "ObjectPath_EndWithPeriod", "Object Path may not end with .");
			}
			return false;
		}

		PackageName = InObjectPath.Mid(0, ObjectDelimiterIdx);
		RemainingObjectPath = InObjectPath.Mid(ObjectDelimiterIdx + 1);
	}
	else
	{
		PackageName = InObjectPath;
	}

	if (!IsValidLongPackageName(PackageName, true, OutReason))
	{
		return false;
	}

	if (RemainingObjectPath.Len() > 0)
	{
		FText PathContext = NSLOCTEXT("Core", "ObjectPathContext", "Object Path");
		if (!FName::IsValidXName(RemainingObjectPath, INVALID_OBJECTPATH_CHARACTERS, OutReason, &PathContext))
		{
			return false;
		}

		TCHAR LastChar = RemainingObjectPath[RemainingObjectPath.Len() - 1];
		if (LastChar == '.' || LastChar == ':')
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("Core", "ObjectPath_PathWithTrailingSeperator", "Object Path may not end with : or .");
			}
			return false;
		}

		int32 SlashIndex;
		if (RemainingObjectPath.FindChar(TEXT('/'), SlashIndex))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("Core", "ObjectPath_SlashAfterPeriod", "Object Path may not have / after first .");
			}

			return false;
		}
	}

	return true;
}

bool FPackageName::IsValidPath(const FString& InPath)
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	FReadScopeLock ScopeLock(Paths.MountLock);
	for (const FPathPair& Pair : Paths.ContentRootToPath)
	{
		if (FPathViews::IsParentPathOf(Pair.RootPath, InPath))
		{
			return true;
		}
	}

	// The root folder is not handled in the above cases
	return false;
}

void FPackageName::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	FLongPackagePathsSingleton::Get().InsertMountPoint(RootPath, ContentPath);
}

void FPackageName::UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	FLongPackagePathsSingleton::Get().RemoveMountPoint(RootPath, ContentPath);
}

bool FPackageName::MountPointExists(const FString& RootPath)
{
	return FLongPackagePathsSingleton::Get().MountPointExists(RootPath);
}

FName FPackageName::GetPackageMountPoint(const FString& InPackagePath, bool InWithoutSlashes)
{
	FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	
	FReadScopeLock ScopeLock(Paths.MountLock);
	const TArray<FString>& MountPoints = Paths.GetValidLongPackageRoots(true);

	int32 WithoutSlashes = InWithoutSlashes ? 1 : 0;
	for (auto RootIt = MountPoints.CreateConstIterator(); RootIt; ++RootIt)
	{
		if (FPathViews::IsParentPathOf(*RootIt, InPackagePath))
		{
			return FName(*RootIt->Mid(WithoutSlashes, RootIt->Len() - (2 * WithoutSlashes)));
		}
	}

	return FName();
}

bool FPackageName::TryConvertToMountedPathComponents(FStringView InFilePathOrPackageName,
	FStringBuilderBase& OutMountPointPackageName, FStringBuilderBase& OutMountPointFilePath,
	FStringBuilderBase& OutRelPath, FStringBuilderBase& OutObjectName, EPackageExtension& OutExtension,
	FStringBuilderBase& OutCustomExtension, EFlexNameType* OutFlexNameType, EErrorCode* OutFailureReason)
{
	auto ClearSuccessOutputs = [&OutMountPointPackageName, &OutMountPointFilePath, &OutRelPath,
		&OutObjectName, &OutExtension, &OutCustomExtension, OutFlexNameType]()
	{
		OutMountPointPackageName.Reset();
		OutMountPointFilePath.Reset();
		OutRelPath.Reset();
		OutObjectName.Reset();
		OutExtension = EPackageExtension::Unspecified;
		OutCustomExtension.Reset();
		if (OutFlexNameType) *OutFlexNameType = EFlexNameType::Invalid;
	};

	EFlexNameType PathFlexNameType;
	bool bResult = TryGetMountPointForPath(InFilePathOrPackageName, OutMountPointPackageName,
		OutMountPointFilePath, OutRelPath, &PathFlexNameType, OutFailureReason);
	if (!bResult)
	{
		// Complain about spaces in the name before pathnotmounted.
		// "Texture2D /Engine/Foo" should be a spaces error, not a PathNotMounted error.
		int32 UnusedIndex;
		if (OutFailureReason && *OutFailureReason == EErrorCode::PackageNamePathNotMounted &&
			InFilePathOrPackageName.FindChar(TEXT(' '), UnusedIndex))
		{
			*OutFailureReason = EErrorCode::PackageNameSpacesNotAllowed;
		}
		ClearSuccessOutputs();
		return false;
	}

	OutObjectName.Reset();
	OutExtension = EPackageExtension::Unspecified;
	OutCustomExtension.Reset();

	if (PathFlexNameType == EFlexNameType::LocalPath)
	{
		// Remove Extension from OutRelPath and put it into OutExtension
		int32 ExtensionStart;
		OutExtension = FPackagePath::ParseExtension(OutRelPath, &ExtensionStart);
		if (OutExtension == EPackageExtension::Custom)
		{
			OutCustomExtension << FStringView(OutRelPath).RightChop(ExtensionStart);
		}
		OutRelPath.RemoveSuffix(OutRelPath.Len() - ExtensionStart);

	}
	else if (PathFlexNameType == EFlexNameType::ObjectPath)
	{
		// Legacy behavior; convert ObjectPaths to packageName
		TStringBuilder<256> ObjectPath;
		ObjectPath << OutMountPointPackageName << OutRelPath;
		FStringView ClassName;
		FStringView PackageName;
		FStringView ObjectName;
		FStringView SubObjectName;
		SplitFullObjectPath(ObjectPath, ClassName, PackageName, ObjectName, SubObjectName);
		if (ClassName.Len() > 0)
		{
			// If there is no classname, but the packagename or objectname has spaces in the name (which is invalid),
			// it will reach this location as well. This function does not need to care about spaces in the objectname;
			// silently let that invalidity pass.
			// Test whether we are in the case of no class, but spaces in the objectname, by checking whether the PackageName
			// with class detection is invalid, but the packagename without class detection is valid.
			bool bObjectNameErrorCase = false;
			if (!IsValidTextForLongPackageName(PackageName))
			{
				SplitFullObjectPath(ObjectPath, ClassName, PackageName, ObjectName, SubObjectName, false /* bDetectClassName */);
				if (IsValidTextForLongPackageName(PackageName))
				{
					bObjectNameErrorCase = true;
				}
			}
			if (!bObjectNameErrorCase)
			{
				// It's not the object error case, so it's either a fullobjectpath with class or it is a packagename with spaces
				// Both of those are unrecoverable errors for this function, and we can't easily distinguish them. Report
				// the error as spaces are invalid.
				ClearSuccessOutputs();
				if (OutFailureReason) *OutFailureReason = EErrorCode::PackageNameSpacesNotAllowed;
				return false;
			}
		}

		if (!IsValidTextForLongPackageName(PackageName, OutFailureReason))
		{
			ClearSuccessOutputs();
			return false;
		}
		OutObjectName << ObjectName;
		if (SubObjectName.Len())
		{
			OutObjectName << SUBOBJECT_DELIMITER << SubObjectName;
		}
		check(FStringView(PackageName).StartsWith(OutMountPointPackageName));
		int32 RelPathPackageNameLen = PackageName.Len() - OutMountPointPackageName.Len();
		OutRelPath.RemoveSuffix(OutRelPath.Len() - RelPathPackageNameLen);
	}
	else
	{
		check(PathFlexNameType == EFlexNameType::PackageName);
		TStringBuilder<256> PackageNameBuffer;
		PackageNameBuffer << OutMountPointPackageName << OutRelPath;
		FStringView PackageName(PackageNameBuffer);
		FStringView PackageNameNoTrailingSlash = PackageName;
		if (PackageName.Len() > 0 && FPathViews::IsSeparator(PackageName[PackageName.Len() - 1]))
		{
			// IsValidTextForLongPackageName rejects packagenames with a trailing slash, but we want to allow that
			// because this function allows both files and directories
			PackageNameNoTrailingSlash.LeftChopInline(1);
		}
		int32 UnusedIndex;
		if (PackageNameNoTrailingSlash.FindChar(TEXT(' '), UnusedIndex))
		{
			ClearSuccessOutputs();
			if (OutFailureReason) *OutFailureReason = EErrorCode::PackageNameSpacesNotAllowed;
			return false;
		}
		else if (!IsValidTextForLongPackageName(PackageNameNoTrailingSlash, OutFailureReason))
		{
			ClearSuccessOutputs();
			return false;
		}
	}

	if (OutFlexNameType) *OutFlexNameType = PathFlexNameType;
	if (OutFailureReason) *OutFailureReason = EErrorCode::PackageNameUnknown;
	return true;
}

bool FPackageName::TryGetMountPointForPath(FStringView InFilePathOrPackageName, FStringBuilderBase& OutMountPointPackageName, FStringBuilderBase& OutMountPointFilePath, FStringBuilderBase& OutRelPath, EFlexNameType* OutFlexNameType, EErrorCode* OutFailureReason)
{
	OutMountPointPackageName.Reset();
	OutMountPointFilePath.Reset();
	OutRelPath.Reset();

	if (InFilePathOrPackageName.IsEmpty())
	{
		if (OutFlexNameType)
		{
			*OutFlexNameType = EFlexNameType::Invalid;
		}
		if (OutFailureReason)
		{
			*OutFailureReason = EErrorCode::PackageNameEmptyPath;
		}
		return false;
	}

	TStringBuilder<256> PossibleAbsFilePath;
	FPathViews::ToAbsolutePath(InFilePathOrPackageName, PossibleAbsFilePath);
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	FReadScopeLock ScopeLock(Paths.MountLock);
	for (const auto& Pair : Paths.ContentRootToPath)
	{
		FStringView RelPath;
		if (FPathViews::TryMakeChildPathRelativeTo(InFilePathOrPackageName, Pair.RootPath, RelPath))
		{
			OutMountPointPackageName << Pair.RootPath;
			OutMountPointFilePath << Pair.ContentPath;
			OutRelPath << RelPath;
			if (OutFlexNameType)
			{
				if (Algo::Find(RelPath, TEXT('.')))
				{
					*OutFlexNameType = EFlexNameType::ObjectPath;
				}
				else
				{
					*OutFlexNameType = EFlexNameType::PackageName;
				}
			}
			if (OutFailureReason)
			{
				*OutFailureReason = EErrorCode::PackageNameUnknown;
			}
			return true;
		}
		else if (FPathViews::TryMakeChildPathRelativeTo(PossibleAbsFilePath, Pair.AbsolutePath, RelPath))
		{
			OutMountPointPackageName << Pair.RootPath;
			OutMountPointFilePath << Pair.ContentPath;
			OutRelPath << RelPath;
			if (OutFlexNameType)
			{
				*OutFlexNameType = EFlexNameType::LocalPath;
			}
			if (OutFailureReason)
			{
				*OutFailureReason = EErrorCode::PackageNameUnknown;
			}
			return true;
		}
	}
	if (OutFlexNameType)
	{
		*OutFlexNameType = EFlexNameType::Invalid;
	}
	if (OutFailureReason)
	{
		FStringView RelPath;
		if (FPathViews::TryMakeChildPathRelativeTo(InFilePathOrPackageName, Paths.MemoryRootPath, RelPath))
		{
			*OutFailureReason = EErrorCode::PackageNamePathIsMemoryOnly;
		}
		else
		{
			*OutFailureReason = EErrorCode::PackageNamePathNotMounted;
		}
	}
	return false;
}

FString FPackageName::GetModuleScriptPackageName(FStringView InModuleName)
{
	return FString::Printf(TEXT("/Script/%.*s"), InModuleName.Len(), InModuleName.GetData());
}

FName FPackageName::GetModuleScriptPackageName(FName InModuleName)
{
	return FName(WriteToString<128>(TEXT("/Script/"), InModuleName));
}

FString FPackageName::ConvertToLongScriptPackageName(const TCHAR* InShortName)
{
	if (IsShortPackageName(FString(InShortName)))
	{
		return GetModuleScriptPackageName(FStringView(InShortName));
	}
	else
	{
		return InShortName;
	}
}

// Short to long script package name map.
static TMap<FName, FName> ScriptPackageNames;


// @todo: This stuff needs to be eliminated as soon as we can make sure that no legacy short package names
//        are in use when referencing class names in UObject module "class packages"
void FPackageName::RegisterShortPackageNamesForUObjectModules()
{
	// @todo: Ideally we'd only be processing UObject modules, not every module, but we have
	//        no way of knowing which modules may contain UObjects (without say, having UBT save a manifest.)
	// @todo: This stuff is a bomb waiting to explode.  Because short package names can
	//        take precedent over other object names, modules can reserve names for other types!
	TArray<FName> AllModuleNames;
	FModuleManager::Get().FindModules( TEXT( "*" ), AllModuleNames );
	for( TArray<FName>::TConstIterator ModuleNameIt( AllModuleNames ); ModuleNameIt; ++ModuleNameIt )
	{
		ScriptPackageNames.Add(*ModuleNameIt, GetModuleScriptPackageName(*ModuleNameIt));
	}
}

FName* FPackageName::FindScriptPackageName(FName InShortName)
{
	return ScriptPackageNames.Find(InShortName);
}

bool FPackageName::FindPackageFileWithoutExtension(const FString& InPackageFilename, FString& OutFilename, bool InAllowTextFormats)
{
	bool bExists = FindPackageFileWithoutExtension(InPackageFilename, OutFilename);
	if (!InAllowTextFormats)
	{
		FPackagePath PackagePath = FPackagePath::FromLocalPath(OutFilename);
		FOpenPackageResult Result = IPackageResourceManager::Get().OpenReadPackage(PackagePath);
		if (!Result.Archive.IsValid() || Result.Format == EPackageFormat::Text)
		{
			return false;
		}
	}
	return bExists;
}

bool FPackageName::FindPackageFileWithoutExtension(const FString& InPackageFilename, FString& OutFilename)
{
	FPackagePath PackagePath = FPackagePath::FromLocalPath(InPackageFilename);
	if (IPackageResourceManager::Get().DoesPackageExist(PackagePath, &PackagePath))
	{
		OutFilename = PackagePath.GetLocalFullPath();
		return true;
	}
	else
	{
		return false;
	}
}

bool FPackageName::FixPackageNameCase(FString& LongPackageName, FStringView Extension)
{
	FPackagePath PackagePath;
	if (!FPackagePath::TryFromPackageName(LongPackageName, PackagePath))
	{
		return false;
	}
	if (!IPackageResourceManager::Get().TryMatchCaseOnDisk(PackagePath, &PackagePath))
	{
		return false;
	}
	TStringBuilder<256> DiskPackageName;
	PackagePath.AppendPackageName(DiskPackageName);
	check(FStringView(LongPackageName).Equals(DiskPackageName, ESearchCase::IgnoreCase));
	LongPackageName = DiskPackageName;
	return true;
}

bool FPackageName::DoesPackageExist(const FString& LongPackageName, FString* OutFilename, bool InAllowTextFormats)
{
	// Make sure interpreting LongPackageName as a filename is supported.
	FPackagePath PackagePath;
	{
		SCOPED_LOADTIMER(FPackageName_DoesPackageExist);
		TStringBuilder<64> PackageNameRoot;
		TStringBuilder<64> FilePathRoot;
		TStringBuilder<256> RelPath;
		TStringBuilder<64> UnusedObjectName; // DoesPackageExist accepts ObjectPaths and ignores the ObjectName portion and uses only the PackageName
		TStringBuilder<16> CustomExtension;
		EPackageExtension Extension;
		EErrorCode FailureReason;
		if (!FPackageName::TryConvertToMountedPathComponents(LongPackageName, PackageNameRoot, FilePathRoot, RelPath, UnusedObjectName, Extension, CustomExtension, nullptr /* OutFlexNameType */, &FailureReason))
		{
			FString Message = FString::Printf(TEXT("DoesPackageExist called on PackageName that will always return false. Reason: %s"), *FormatErrorAsString(LongPackageName, FailureReason));
			UE_LOG(LogPackageName, Warning, TEXT("%s"), *Message);
			return false;
		}
		PackagePath = FPackagePath::FromMountedComponents(PackageNameRoot, FilePathRoot, RelPath, Extension, CustomExtension);
	}
	if (!DoesPackageExist(PackagePath, false /* bMatchCaseOnDisk */, &PackagePath))
	{
		return false;
	}
	if (!InAllowTextFormats && IsTextPackageExtension(PackagePath.GetHeaderExtension()))
	{
		return false;
	}
	if (OutFilename)
	{
		*OutFilename = PackagePath.GetLocalFullPath();
	}
	return true;
}

bool FPackageName::DoesPackageExist(const FPackagePath& PackagePath, FPackagePath* OutPackagePath)
{
	return DoesPackageExist(PackagePath, false, OutPackagePath);
}

bool FPackageName::DoesPackageExist(const FPackagePath& PackagePath, bool bMatchCaseOnDisk, FPackagePath* OutPackagePath)
{
	return DoesPackageExistEx(PackagePath, EPackageLocationFilter::Any, bMatchCaseOnDisk, OutPackagePath) != EPackageLocationFilter::None;
}

FPackageName::EPackageLocationFilter FPackageName::DoesPackageExistEx(const FPackagePath& PackagePath, EPackageLocationFilter Filter, bool bMatchCaseOnDisk, FPackagePath* OutPackagePath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPackageName::DoesPackageExistEx);

	// DoesPackageExist returns false for local filenames that are in unmounted directories, even if those files exist on the local disk
	if (!PackagePath.IsMountedPath())
	{
		return EPackageLocationFilter::None;
	}
	TStringBuilder<256> PackageName;
	PackagePath.AppendPackageName(PackageName);

	// Once we have the real Package Name, we can exit early if it's a script package - they exist only in memory.
	if (IsScriptPackage(PackageName))
	{
		return EPackageLocationFilter::None;
	}

	if (IsMemoryPackage(PackageName))
	{
		return EPackageLocationFilter::None;
	}

	FText Reason;
	if ( !FPackageName::IsValidTextForLongPackageName( PackageName, &Reason ) )
	{
		UE_LOG(LogPackageName, Error, TEXT( "DoesPackageExist: DoesPackageExist FAILED: '%s' is not a long packagename name. Reason: %s"), PackageName.ToString(), *Reason.ToString() );
		return EPackageLocationFilter::None;
	}

	EPackageLocationFilter Result = EPackageLocationFilter::None;

	if ((uint8)Filter & (uint8)EPackageLocationFilter::FileSystem)
	{
		bool bFoundInFileSystem = false;
		if (bMatchCaseOnDisk)
		{
			bFoundInFileSystem = IPackageResourceManager::Get().TryMatchCaseOnDisk(PackagePath, OutPackagePath);
		}
		else
		{
			bFoundInFileSystem = IPackageResourceManager::Get().DoesPackageExist(PackagePath, OutPackagePath);
		}

		if (bFoundInFileSystem)
		{
			Result = EPackageLocationFilter((uint8)Result | (uint8)EPackageLocationFilter::FileSystem);

			// if we just want to find any existence, then we are done
			if (Filter == EPackageLocationFilter::Any)
			{
				return Result;
			}
		}
	}

	if (((uint8)Filter & (uint8)EPackageLocationFilter::IoDispatcher))
	{
		bool bFoundInIoDispatcher = false;
		if (DoesPackageExistOverrideDelegate.IsBound())
		{
			if (DoesPackageExistOverrideDelegate.Execute(PackagePath.GetPackageFName()))
			{
				bFoundInIoDispatcher = true;
			}
		}
		else if (FIoDispatcher::IsInitialized())
		{
			bFoundInIoDispatcher = FIoDispatcher::Get().DoesChunkExist(CreatePackageDataChunkId(FPackageId::FromName(PackagePath.GetPackageFName())));
		}

		if (bFoundInIoDispatcher)
		{
			if (OutPackagePath)
			{
				*OutPackagePath = PackagePath;
				if (OutPackagePath->GetHeaderExtension() == EPackageExtension::Unspecified)
				{
					OutPackagePath->SetHeaderExtension(EPackageExtension::EmptyString);
				}
			}

			Result = EPackageLocationFilter((uint8)Result | (uint8)EPackageLocationFilter::IoDispatcher);

			// if we just want to find any existence, then we are done
			if (Filter == EPackageLocationFilter::Any)
			{
				return Result;
			}
		}
	}

	return Result;
}

bool FPackageName::SearchForPackageOnDisk(const FString& PackageName, FString* OutLongPackageName, FString* OutFilename)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPackageName::SearchForPackageOnDisk);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FPackageName::SearchForPackageOnDisk"), STAT_PackageName_SearchForPackageOnDisk, STATGROUP_LoadTime);

	// This function may take a long time to complete, so suspend heartbeat measure while we're here
	FSlowHeartBeatScope SlowHeartBeatScope;

	bool bResult = false;
	double StartTime = FPlatformTime::Seconds();
	if (FPackageName::IsShortPackageName(PackageName) == false)
	{
		// If this is long package name, revert to using DoesPackageExist because it's a lot faster.
		FString Filename;
		if (DoesPackageExist(PackageName, &Filename))
		{
			if (OutLongPackageName)
			{
				*OutLongPackageName = PackageName;
			}
			if (OutFilename)
			{
				*OutFilename = Filename;
			}
			bResult = true;
		}
	}
	else
	{
		// Attempt to find package by its short name by searching in the known content paths.
		TArray<TPair<FString,FString>> RootsNameAndFile;
		{
			TArray<FString> RootContentPaths;
			FPackageName::QueryRootContentPaths( RootContentPaths );
			for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
			{
				const FString& RootPackageName = *RootPathIt;
				const FString& RootFilePath = FPackageName::LongPackageNameToFilename(RootPackageName, TEXT(""));
				RootsNameAndFile.Add(TPair<FString,FString>(RootPackageName, RootFilePath));
			}
		}

		int32 ExtensionStart;
		EPackageExtension RequiredExtension = FPackagePath::ParseExtension(PackageName, &ExtensionStart);
		if (RequiredExtension == EPackageExtension::Custom || ExtensionToSegment(RequiredExtension) != EPackageSegment::Header)
		{
			UE_LOG(LogPackageName, Warning, TEXT("SearchForPackageOnDisk: Invalid extension in packagename %s. Searching for any header extension instead."), *PackageName);
			RequiredExtension = EPackageExtension::Unspecified;
		}
		TStringBuilder<128> PackageWildCard;
		PackageWildCard << FStringView(PackageName).Left(ExtensionStart) << TEXT(".*");

		FPackagePath FirstResult;
		TArray<FPackagePath> FoundResults;
		IPackageResourceManager& PackageResourceManager = IPackageResourceManager::Get();
		for (const TPair<FString, FString>& RootNameAndFile : RootsNameAndFile)
		{
			const FString& RootPackageName = RootNameAndFile.Get<0>();
			const FString& RootFilePath = RootNameAndFile.Get<1>();
			check(RootPackageName.EndsWith(TEXT("/")));
			check(RootFilePath.EndsWith(TEXT("/")));
			// Search directly on disk. Very slow!
			FoundResults.Reset();
			PackageResourceManager.FindPackagesRecursive(FoundResults, RootPackageName, RootFilePath, FStringView(), PackageWildCard);

			for (const FPackagePath& FoundPackagePath: FoundResults)
			{			
				FStringView UnusedCustomExtension;
				if (RequiredExtension != EPackageExtension::Unspecified && FoundPackagePath.GetHeaderExtension() != RequiredExtension)
				{
					continue;
				}

				bResult = true;
				if (OutLongPackageName || OutFilename)
				{
					if (!FirstResult.IsEmpty())
					{
						UE_LOG(LogPackageName, Warning, TEXT("SearchForPackageOnDisk: Found ambiguous long package name for '%s'. Returning '%s', but could also be '%s'."), *PackageName,
							*FirstResult.GetDebugNameWithExtension(), *FoundPackagePath.GetDebugNameWithExtension());
					}
					else
					{
						if (OutLongPackageName)
						{
							*OutLongPackageName = FoundPackagePath.GetPackageName();
						}
						if (OutFilename)
						{
							*OutFilename = FoundPackagePath.GetLocalFullPath();
						}
						FirstResult = FoundPackagePath;
					}
				}
			}
			if (bResult)
			{
				break;
			}
		}
	}
	const double ThisTime = FPlatformTime::Seconds() - StartTime;

	if ( bResult )
	{
		UE_LOG(LogPackageName, Log, TEXT("SearchForPackageOnDisk took %7.3fs to resolve %s."), ThisTime, *PackageName);
	}
	else
	{
		UE_LOG(LogPackageName, Log, TEXT("SearchForPackageOnDisk took %7.3fs, but failed to resolve %s."), ThisTime, *PackageName);
	}

	return bResult;
}

bool FPackageName::TryConvertShortPackagePathToLongInObjectPath(const FString& ObjectPath, FString& ConvertedObjectPath)
{
	FString PackagePath;
	FString ObjectName;

	int32 DotPosition = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive);
	if (DotPosition != INDEX_NONE)
	{
		PackagePath = ObjectPath.Mid(0, DotPosition);
		ObjectName = ObjectPath.Mid(DotPosition + 1);
	}
	else
	{
		PackagePath = ObjectPath;
	}

	FString LongPackagePath;
	if (!SearchForPackageOnDisk(PackagePath, &LongPackagePath))
	{
		return false;
	}

	ConvertedObjectPath = FString::Printf(TEXT("%s.%s"), *LongPackagePath, *ObjectName);
	return true;
}

FString FPackageName::GetNormalizedObjectPath(const FString& ObjectPath)
{
	if (!ObjectPath.IsEmpty() && FPackageName::IsShortPackageName(ObjectPath))
	{
		FString LongPath;

		UE_LOG(LogPackageName, Warning, TEXT("Asset path \"%s\" is in short form, which is unsupported and -- even if valid -- resolving it will be really slow."), *ObjectPath);
		UE_LOG(LogPackageName, Warning, TEXT("Please consider resaving package in order to speed-up loading."));
		
		if (!FPackageName::TryConvertShortPackagePathToLongInObjectPath(ObjectPath, LongPath))
		{
			UE_LOG(LogPackageName, Warning, TEXT("Asset path \"%s\" could not be resolved."), *ObjectPath);
		}

		return LongPath;
	}
	else
	{
		return ObjectPath;
	}
}

FString FPackageName::GetDelegateResolvedPackagePath(const FString& InSourcePackagePath)
{
	if (FCoreDelegates::PackageNameResolvers.Num() > 0)
	{
		bool WasResolved = false;

		// If the path is /Game/Path/Foo.Foo only worry about resolving the /Game/Path/Foo
		FString PathName = InSourcePackagePath;
		FString ObjectName;
		int32 DotIndex = INDEX_NONE;

		if (PathName.FindChar(TEXT('.'), DotIndex))
		{
			ObjectName = PathName.Mid(DotIndex + 1);
			PathName.LeftInline(DotIndex, false);
		}

		for (auto Delegate : FCoreDelegates::PackageNameResolvers)
		{
			FString ResolvedPath;
			if (Delegate.Execute(PathName, ResolvedPath))
			{
				UE_LOG(LogPackageName, Display, TEXT("Package '%s' was resolved to '%s'"), *PathName, *ResolvedPath);
				PathName = ResolvedPath;
				WasResolved = true;
			}
		}

		if (WasResolved)
		{
			// If package was passed in with an object, add that back on by deriving it from the package name
			if (ObjectName.Len())
			{
				int32 LastSlashIndex = INDEX_NONE;
				if (PathName.FindLastChar(TEXT('/'), LastSlashIndex))
				{
					ObjectName = PathName.Mid(LastSlashIndex + 1);
				}

				PathName += TEXT(".");
				PathName += ObjectName;
			}

			return PathName;
		}
	}

	return InSourcePackagePath;
}

FString FPackageName::GetSourcePackagePath(const FString& InLocalizedPackagePath)
{
	// This function finds the start and end point of the "/L10N/<culture>" part of the path so that it can be removed
	auto GetL10NTrimRange = [](const FString& InPath, int32& OutL10NStart, int32& OutL10NLength)
	{
		const TCHAR* CurChar = *InPath;

		// Must start with a slash
		if (*CurChar++ != TEXT('/'))
		{
			return false;
		}

		// Find the end of the first part of the path, eg /Game/
		while (*CurChar && *CurChar++ != TEXT('/')) {}
		if (!*CurChar)
		{
			// Found end-of-string
			return false;
		}

		if (FCString::Strnicmp(CurChar, TEXT("L10N/"), 5) == 0) // StartsWith "L10N/"
		{
			CurChar -= 1; // -1 because we need to eat the slash before L10N
			OutL10NStart = UE_PTRDIFF_TO_INT32(CurChar - *InPath);
			OutL10NLength = 6; // "/L10N/"

			// Walk to the next slash as that will be the end of the culture code
			CurChar += OutL10NLength;
			while (*CurChar && *CurChar++ != TEXT('/')) { ++OutL10NLength; }

			return true;
		}
		else if (FCString::Stricmp(CurChar, TEXT("L10N")) == 0) // Is "L10N"
		{
			CurChar -= 1; // -1 because we need to eat the slash before L10N
			OutL10NStart = UE_PTRDIFF_TO_INT32(CurChar - *InPath);
			OutL10NLength = 5; // "/L10N"

			return true;
		}

		return false;
	};

	FString SourcePackagePath = InLocalizedPackagePath;

	int32 L10NStart = INDEX_NONE;
	int32 L10NLength = 0;
	if (GetL10NTrimRange(SourcePackagePath, L10NStart, L10NLength))
	{
		SourcePackagePath.RemoveAt(L10NStart, L10NLength);
	}

	return SourcePackagePath;
}

FString FPackageName::GetLocalizedPackagePath(const FString& InSourcePackagePath)
{
	const FName LocalizedPackageName = FPackageLocalizationManager::Get().FindLocalizedPackageName(*InSourcePackagePath);
	return (LocalizedPackageName.IsNone()) ? InSourcePackagePath : LocalizedPackageName.ToString();
}

FString FPackageName::GetLocalizedPackagePath(const FString& InSourcePackagePath, const FString& InCultureName)
{
	const FName LocalizedPackageName = FPackageLocalizationManager::Get().FindLocalizedPackageNameForCulture(*InSourcePackagePath, InCultureName);
	return (LocalizedPackageName.IsNone()) ? InSourcePackagePath : LocalizedPackageName.ToString();
}

const FString& FPackageName::GetAssetPackageExtension()
{
	static FString AssetPackageExtension(LexToString(EPackageExtension::Asset));
	return AssetPackageExtension;
}

const FString& FPackageName::GetMapPackageExtension()
{
	static FString MapPackageExtension(LexToString(EPackageExtension::Map));
	return MapPackageExtension;
}

const FString& FPackageName::GetTextAssetPackageExtension()
{
	static FString TextAssetPackageExtension(LexToString(EPackageExtension::TextAsset));
	return TextAssetPackageExtension;
}

const FString& FPackageName::GetTextMapPackageExtension()
{
	static FString TextMapPackageExtension(LexToString(EPackageExtension::TextMap));
	return TextMapPackageExtension;
}

bool FPackageName::IsTextPackageExtension(const TCHAR* Ext)
{
	return IsTextAssetPackageExtension(Ext) || IsTextMapPackageExtension(Ext);
}

bool FPackageName::IsTextPackageExtension(EPackageExtension Extension)
{
	return Extension == EPackageExtension::TextAsset || Extension == EPackageExtension::TextMap;
}

bool FPackageName::IsTextAssetPackageExtension(const TCHAR* Ext)
{
	FStringView TextAssetPackageExtension(LexToString(EPackageExtension::TextAsset));
	if (*Ext != TEXT('.') && *Ext != TEXT('\0'))
	{
		return (TextAssetPackageExtension.RightChop(1) == Ext);
	}
	else
	{
		return (TextAssetPackageExtension == Ext);
	}
}

bool FPackageName::IsTextMapPackageExtension(const TCHAR* Ext)
{
	FStringView TextMapPackageExtension(LexToString(EPackageExtension::TextMap));
	if (*Ext != TEXT('.') && *Ext != TEXT('\0'))
	{
		return (TextMapPackageExtension.RightChop(1) == Ext);
	}
	else
	{
		return (TextMapPackageExtension == Ext);
	}
}

bool FPackageName::IsPackageExtension( const TCHAR* Ext )
{
	return IsAssetPackageExtension(Ext) || IsMapPackageExtension(Ext);
}

bool FPackageName::IsPackageExtension(EPackageExtension Extension)
{
	return Extension == EPackageExtension::Asset || Extension == EPackageExtension::Map;
}

bool FPackageName::IsAssetPackageExtension(const TCHAR* Ext)
{
	FStringView AssetPackageExtension(LexToString(EPackageExtension::Asset));
	if (*Ext != TEXT('.'))
	{
		return (AssetPackageExtension.RightChop(1) == Ext);
	}
	else
	{
		return (AssetPackageExtension == Ext);
	}
}

bool FPackageName::IsMapPackageExtension(const TCHAR* Ext)
{
	FStringView MapPackageExtension(LexToString(EPackageExtension::Map));
	if (*Ext != TEXT('.'))
	{
		return (MapPackageExtension.RightChop(1) == Ext);
	}
	else
	{
		return (MapPackageExtension == Ext);
	}
}

bool FPackageName::FindPackagesInDirectory( TArray<FString>& OutPackages, const FString& RootDir )
{
	// Keep track if any package has been found. Can't rely only on OutPackages.Num() > 0 as it may not be empty.
	const int32 PreviousPackagesCount = OutPackages.Num();
	IteratePackagesInDirectory(RootDir, [&OutPackages](const TCHAR* PackageFilename) -> bool
	{
		OutPackages.Add(PackageFilename);
		return true;
	});
	return OutPackages.Num() > PreviousPackagesCount;
}

bool FPackageName::FindPackagesInDirectories(TArray<FString>& OutPackages, const TArrayView<const FString>& RootDirs)
{
	TSet<FString> Packages;
	TArray<FString> DirPackages;
	for (const FString& RootDir : RootDirs)
	{
		DirPackages.Reset();
		FindPackagesInDirectory(DirPackages, RootDir);
		for (FString& DirPackage : DirPackages)
		{
			Packages.Add(MoveTemp(DirPackage));
		}
	}
	OutPackages.Reserve(Packages.Num() + OutPackages.Num());
	for (FString& Package : Packages)
	{
		OutPackages.Add(MoveTemp(Package));
	}
	return Packages.Num() > 0;
}

void FPackageName::IteratePackagesInDirectory(const FString& RootDir, const FPackageNameVisitor& Callback)
{
	auto LocalCallback = [&Callback](const FPackagePath& PackagePath) -> bool
	{
		return Callback(*PackagePath.GetLocalFullPath());
	};

	TStringBuilder<256> PackageNameRoot;
	TStringBuilder<256> FilePathRoot;
	TStringBuilder<256> RelRootDir;
	if (TryGetMountPointForPath(RootDir, PackageNameRoot, FilePathRoot, RelRootDir))
	{
		IPackageResourceManager::Get().IteratePackagesInPath(PackageNameRoot, FilePathRoot, RelRootDir, LocalCallback);
	}
	else
	{
		// Searching a localonly path
		IPackageResourceManager::Get().IteratePackagesInLocalOnlyDirectory(RootDir, LocalCallback);
	}
}

void FPackageName::IteratePackagesInDirectory(const FString& RootDir, const FPackageNameStatVisitor& Callback)
{
	auto LocalCallback = [&Callback](const FPackagePath& PackagePath, const FFileStatData& StatData) -> bool
	{
		return Callback(*PackagePath.GetLocalFullPath(), StatData);
	};

	TStringBuilder<256> PackageNameRoot;
	TStringBuilder<256> FilePathRoot;
	TStringBuilder<256> RelRootDir;
	if (TryGetMountPointForPath(RootDir, PackageNameRoot, FilePathRoot, RelRootDir))
	{
		IPackageResourceManager::Get().IteratePackagesStatInPath(PackageNameRoot, FilePathRoot, RelRootDir, LocalCallback);
	}
	else
	{
		// Searching a localonly path
		IPackageResourceManager::Get().IteratePackagesStatInLocalOnlyDirectory(RootDir, LocalCallback);
	}
}

void FPackageName::QueryRootContentPaths(TArray<FString>& OutRootContentPaths, bool bIncludeReadOnlyRoots, bool bWithoutLeadingSlashes, bool bWithoutTrailingSlashes)
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();

	{
		FReadScopeLock ScopeLock(Paths.MountLock);
		OutRootContentPaths = Paths.GetValidLongPackageRoots( bIncludeReadOnlyRoots );
	}

	if (bWithoutTrailingSlashes || bWithoutLeadingSlashes)
	{
		for (FString& It : OutRootContentPaths)
		{
			if (bWithoutTrailingSlashes && It.Len() > 1 && It[It.Len() - 1] == TEXT('/'))
			{
				It.RemoveAt(It.Len() - 1, /*Count*/ 1, /*bAllowShrinking*/ false);
			}

			if (bWithoutLeadingSlashes && It.Len() > 1 && It[0] == TEXT('/'))
			{
				It.RemoveAt(0, /*Count*/ 1, /*bAllowShrinking*/ false);
			}
		}
	}
}

void FPackageName::OnCoreUObjectInitialized()
{
	FLongPackagePathsSingleton::Get().OnCoreUObjectInitialized();
}

template<class T>
bool ParseExportTextPathImpl(const T& InExportTextPath, T* OutClassName, T* OutObjectPath)
{
	int32 Index;
	if (InExportTextPath.FindChar('\'', Index) && InExportTextPath.IsValidIndex(Index + 1) && InExportTextPath[InExportTextPath.Len() - 1] == '\'') // IsValidIndex checks that the found ' isn't the same one that's at the end of the string
	{
		if (OutClassName)
		{
			*OutClassName = InExportTextPath.Left(Index);
		}

		if (OutObjectPath)
		{
			*OutObjectPath = InExportTextPath.Mid(Index + 1, InExportTextPath.Len() - Index - 2); // -2 because we're stripping the first and last '
		}

		return true;
	}
	
	return false;
}

bool FPackageName::ParseExportTextPath(const FString& InExportTextPath, FString* OutClassName, FString* OutObjectPath)
{
	return ParseExportTextPathImpl(InExportTextPath, OutClassName, OutObjectPath);
}

bool FPackageName::ParseExportTextPath(FWideStringView InExportTextPath, FWideStringView* OutClassName, FWideStringView* OutObjectPath)
{
	return ParseExportTextPathImpl(InExportTextPath, OutClassName, OutObjectPath);
}

bool FPackageName::ParseExportTextPath(FAnsiStringView InExportTextPath, FAnsiStringView* OutClassName, FAnsiStringView* OutObjectPath)
{
	return ParseExportTextPathImpl(InExportTextPath, OutClassName, OutObjectPath);
}

bool FPackageName::ParseExportTextPath(const TCHAR* InExportTextPath, FStringView* OutClassName, FStringView* OutObjectPath)
{
	return ParseExportTextPath(FStringView(InExportTextPath), OutClassName, OutObjectPath);
}

template <class T>
T ExportTextPathToObjectPathImpl(const T& InExportTextPath)
{
	T ObjectPath;
	if (FPackageName::ParseExportTextPath(InExportTextPath, nullptr, &ObjectPath))
	{
		return ObjectPath;
	}
	
	// Could not parse the export text path. Could already be an object path, just return it back.
	return InExportTextPath;
}

FWideStringView FPackageName::ExportTextPathToObjectPath(FWideStringView InExportTextPath)
{
	return ExportTextPathToObjectPathImpl(InExportTextPath);
}

FAnsiStringView FPackageName::ExportTextPathToObjectPath(FAnsiStringView InExportTextPath)
{
	return ExportTextPathToObjectPathImpl(InExportTextPath);
}

FString FPackageName::ExportTextPathToObjectPath(const FString& InExportTextPath)
{
	return ExportTextPathToObjectPathImpl(InExportTextPath);
}

FString FPackageName::ExportTextPathToObjectPath(const TCHAR* InExportTextPath)
{
	return ExportTextPathToObjectPath(FString(InExportTextPath));
}

template<class T>
T ObjectPathToPackageNameImpl(const T& InObjectPath)
{
	// Check for package delimiter
	int32 ObjectDelimiterIdx;
	if (InObjectPath.FindChar('.', ObjectDelimiterIdx))
	{
		return InObjectPath.Mid(0, ObjectDelimiterIdx);
	}

	// No object delimiter. The path must refer to the package name directly.
	return InObjectPath;
}

FWideStringView FPackageName::ObjectPathToPackageName(FWideStringView InObjectPath)
{
	return ObjectPathToPackageNameImpl(InObjectPath);
}

FAnsiStringView FPackageName::ObjectPathToPackageName(FAnsiStringView InObjectPath)
{
	return ObjectPathToPackageNameImpl(InObjectPath);
}

FString FPackageName::ObjectPathToPackageName(const FString& InObjectPath)
{
	return ObjectPathToPackageNameImpl(InObjectPath);
}

template<class T>
T ObjectPathToObjectNameImpl(const T& InObjectPath)
{
	// Check for a subobject
	int32 SubObjectDelimiterIdx;
	if ( InObjectPath.FindChar(':', SubObjectDelimiterIdx) )
	{
		return InObjectPath.Mid(SubObjectDelimiterIdx + 1);
	}

	// Check for a top level object
	int32 ObjectDelimiterIdx;
	if ( InObjectPath.FindChar('.', ObjectDelimiterIdx) )
	{
		return InObjectPath.Mid(ObjectDelimiterIdx + 1);
	}

	// No object or subobject delimiters. The path must refer to the object name directly (i.e. a package).
	return InObjectPath;
}

FString FPackageName::ObjectPathToObjectName(const FString& InObjectPath)
{
	return ObjectPathToObjectNameImpl(InObjectPath);
}

FWideStringView FPackageName::ObjectPathToObjectName(FWideStringView InObjectPath)
{
	return ObjectPathToObjectNameImpl(InObjectPath);
}

bool FPackageName::IsVersePackage(FStringView InPackageName)
{
	return InPackageName.Contains(FLongPackagePathsSingleton::Get().VerseSubPath);
}

bool FPackageName::IsScriptPackage(FStringView InPackageName)
{
	return FPathViews::IsParentPathOf(FLongPackagePathsSingleton::Get().ScriptRootPath, InPackageName);
}

bool FPackageName::IsMemoryPackage(FStringView InPackageName)
{
	return FPathViews::IsParentPathOf(FLongPackagePathsSingleton::Get().MemoryRootPath, InPackageName);
}

bool FPackageName::IsTempPackage(FStringView InPackageName)
{
	return FPathViews::IsParentPathOf(FLongPackagePathsSingleton::Get().TempRootPath, InPackageName);
}

bool FPackageName::IsLocalizedPackage(FStringView InPackageName)
{
	// Minimum valid package name length is "/A/L10N"
	if (InPackageName.Len() < 7)
	{
		return false;
	}

	const TCHAR* CurChar = InPackageName.GetData();
	const TCHAR* EndChar = InPackageName.GetData() + InPackageName.Len();

	// Must start with a slash
	if (CurChar == EndChar || *CurChar++ != TEXT('/'))
	{
		return false;
	}

	// Find the end of the first part of the path, eg /Game/
	while (CurChar != EndChar && *CurChar++ != TEXT('/')) {}
	if (CurChar == EndChar)
	{
		// Found end-of-string
		return false;
	}

	// Are we part of the L10N folder?
	FStringView Remaining(CurChar, UE_PTRDIFF_TO_INT32(EndChar - CurChar));
	// Is "L10N" or StartsWith "L10N/" 
	return Remaining.StartsWith(TEXTVIEW("L10N"), ESearchCase::IgnoreCase) && (Remaining.Len() == 4 || Remaining[4] == '/');
}

FString FPackageName::FormatErrorAsString(FStringView InPath, EErrorCode ErrorCode)
{
	FText ErrorText = FormatErrorAsText(InPath, ErrorCode);
	return ErrorText.ToString();
}

FText FPackageName::FormatErrorAsText(FStringView InPath, EErrorCode ErrorCode)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("InPath"), FText::FromString(FString(InPath)));
	switch (ErrorCode)
	{
	case EErrorCode::PackageNameUnknown:
		return FText::Format(NSLOCTEXT("Core", "PackageNameUnknownError", "Input '{InPath}' caused undocumented internal error."), Args);
	case EErrorCode::PackageNameEmptyPath:
		return FText::Format(NSLOCTEXT("Core", "PackageNameEmptyPath", "Input '{InPath}' was empty."), Args);
	case EErrorCode::PackageNamePathNotMounted:
		return FText::Format(NSLOCTEXT("Core", "PackageNamePathNotMounted", "Input '{InPath}' is not a child of an existing mount point."), Args);
	case EErrorCode::PackageNamePathIsMemoryOnly:
		return FText::Format(NSLOCTEXT("Core", "PackageNamePathIsMemoryOnly", "Input '{InPath}' is in a memory-only mount point."), Args);
	case EErrorCode::PackageNameSpacesNotAllowed:
		return FText::Format(NSLOCTEXT("Core", "PackageNameSpacesNotAllowed", "Input '{InPath}' includes a space. A space is not valid in ObjectPaths unless it is delimiting a ClassName in a full ObjectPath, and full ObjectPaths are not supported in this function."), Args);
	case EErrorCode::PackageNameContainsInvalidCharacters:
		Args.Add(TEXT("IllegalNameCharacters"), FText::FromString(FString(INVALID_LONGPACKAGE_CHARACTERS)));
		return FText::Format(NSLOCTEXT("Core", "PackageNameContainsInvalidCharacters", "Input '{InPath}' contains one of the invalid characters for LongPackageNames: '{IllegalNameCharacters}'."), Args);
	case EErrorCode::LongPackageNames_PathTooShort:
	{
		// This has to be an FFormatOrderedArguments until we change the localized text string for it.
		FFormatOrderedArguments OrderedArgs;
		OrderedArgs.Add(FText::AsNumber(PackageNameConstants::MinPackageNameLength));
		OrderedArgs.Add(FText::FromString(FString(InPath)));
		return FText::Format(NSLOCTEXT("Core", "LongPackageNames_PathTooShort", "Input '{1}' contains fewer than the minimum number of characters {0} for LongPackageNames."), OrderedArgs);
	}
	case EErrorCode::LongPackageNames_PathWithNoStartingSlash:
		return FText::Format(NSLOCTEXT("Core", "LongPackageNames_PathWithNoStartingSlash", "Input '{InPath}' does not start with a '/', which is required for LongPackageNames."), Args);
	case EErrorCode::LongPackageNames_PathWithTrailingSlash:
		return FText::Format(NSLOCTEXT("Core", "LongPackageNames_PathWithTrailingSlash", "Input '{InPath}' ends with a '/', which is invalid for LongPackageNames."), Args);
	case EErrorCode::LongPackageNames_PathWithDoubleSlash:
		return FText::Format(NSLOCTEXT("Core", "LongPackageNames_PathWithDoubleSlash", "Input '{InPath}' contains '//', which is invalid for LongPackageNames."), Args);
	default:
		UE_LOG(LogPackageName, Warning, TEXT("FPackageName::FormatErrorAsText: Invalid ErrorCode %d"), static_cast<int32>(ErrorCode));
		return FText::Format(NSLOCTEXT("Core", "PackageNameUndocumentedError", "Input '{InPath} caused undocumented internal error."), Args);
	}
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageNameTests, "System.Core.Misc.PackageNames", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPackageNameTests::RunTest(const FString& Parameters)
{
	// Localized paths tests
	{
		auto TestIsLocalizedPackage = [&](const FString& InPath, const bool InExpected)
		{
			const bool bResult = FPackageName::IsLocalizedPackage(InPath);
			if (bResult != InExpected)
			{
				AddError(FString::Printf(TEXT("Path '%s' failed FPackageName::IsLocalizedPackage (got '%d', expected '%d')."), *InPath, bResult, InExpected));
			}
		};
		
		TestIsLocalizedPackage(TEXT("/Game"), false);
		TestIsLocalizedPackage(TEXT("/Game/MyAsset"), false);
		TestIsLocalizedPackage(TEXT("/Game/L10N"), true);
		TestIsLocalizedPackage(TEXT("/Game/L10N/en"), true);
		TestIsLocalizedPackage(TEXT("/Game/L10N/en/MyAsset"), true);
	}

	// Source path tests
	{
		auto TestGetSourcePackagePath = [this](const FString& InPath, const FString& InExpected)
		{
			const FString Result = FPackageName::GetSourcePackagePath(InPath);
			if (Result != InExpected)
			{
				AddError(FString::Printf(TEXT("Path '%s' failed FPackageName::GetSourcePackagePath (got '%s', expected '%s')."), *InPath, *Result, *InExpected));
			}
		};

		TestGetSourcePackagePath(TEXT("/Game"), TEXT("/Game"));
		TestGetSourcePackagePath(TEXT("/Game/MyAsset"), TEXT("/Game/MyAsset"));
		TestGetSourcePackagePath(TEXT("/Game/L10N"), TEXT("/Game"));
		TestGetSourcePackagePath(TEXT("/Game/L10N/en"), TEXT("/Game"));
		TestGetSourcePackagePath(TEXT("/Game/L10N/en/MyAsset"), TEXT("/Game/MyAsset"));
	}

	// TryConvertToMountedPath
	{
		auto TestConvert = [this](FStringView InPath, bool bExpectedResult, FStringView ExpectedLocalPath,
			FStringView ExpectedPackageName, FStringView ExpectedObjectName, FStringView ExpectedSubObjectName,
			FStringView ExpectedExtension, FPackageName::EFlexNameType ExpectedFlexNameType,
			FPackageName::EErrorCode ExpectedFailureReason)
		{
			bool bActualResult;
			FString ActualLocalPath;
			FString ActualPackageName;
			FString ActualObjectName;
			FString ActualSubObjectName;
			FString ActualExtension;
			FPackageName::EFlexNameType ActualFlexNameType;
			FPackageName::EErrorCode ActualFailureReason;

			bActualResult = FPackageName::TryConvertToMountedPath(InPath, &ActualLocalPath, &ActualPackageName,
				&ActualObjectName, &ActualSubObjectName, &ActualExtension, &ActualFlexNameType, &ActualFailureReason);
			if (bActualResult != bExpectedResult || ActualLocalPath != ExpectedLocalPath ||
				ActualPackageName != ExpectedPackageName || ActualObjectName != ExpectedObjectName ||
				ActualSubObjectName != ExpectedSubObjectName || ActualExtension != ExpectedExtension ||
				ActualFlexNameType != ExpectedFlexNameType || ActualFailureReason != ExpectedFailureReason)
			{
				AddError(FString::Printf(TEXT("Path '%.*s' failed FPackageName::TryConvertToMountedPath\n"
					"got      %s,'%s','%s','%s','%s','%s',%d,%d,\nexpected %s,'%.*s','%.*s','%.*s','%.*s','%.*s',%d,%d."),
					InPath.Len(), InPath.GetData(),
					bActualResult ? TEXT("true") : TEXT("false"),
					*ActualLocalPath, *ActualPackageName, *ActualObjectName, *ActualSubObjectName, *ActualExtension,
					(int32)ActualFlexNameType, (int32)ActualFailureReason,
					bExpectedResult ? TEXT("true") : TEXT("false"),
					ExpectedLocalPath.Len(), ExpectedLocalPath.GetData(),
					ExpectedPackageName.Len(), ExpectedPackageName.GetData(),
					ExpectedObjectName.Len(), ExpectedObjectName.GetData(),
					ExpectedSubObjectName.Len(), ExpectedSubObjectName.GetData(),
					ExpectedExtension.Len(), ExpectedExtension.GetData(),
					(int32)ExpectedFlexNameType, (int32)ExpectedFailureReason));
			}
		};
		FString EngineFileRoot = FPaths::EngineContentDir();
		auto EngineLocalPath = [&EngineFileRoot](FStringView RelPath)
		{
			return FPaths::Combine(EngineFileRoot, FString(RelPath));
		};
		TestConvert(TEXT("/Engine"), true, EngineFileRoot, TEXT("/Engine/"),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::PackageName, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(TEXT("/Engine/"), true, EngineFileRoot, TEXT("/Engine/"),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::PackageName, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(EngineFileRoot, true, EngineFileRoot, TEXT("/Engine/"),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::LocalPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(TEXT("/Engine/Foo"), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::PackageName, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(EngineLocalPath(TEXT("Foo")), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::LocalPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(EngineLocalPath(TEXT("Foo.uasset")), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			FStringView(), FStringView(), TEXT(".uasset"),
			FPackageName::EFlexNameType::LocalPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(EngineLocalPath(TEXT("Foo.Bar")), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			FStringView(), FStringView(), TEXT(".Bar"),
			FPackageName::EFlexNameType::LocalPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(TEXT("/Engine/Foo.Bar"), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			TEXT("Bar"), FStringView(), FStringView(),
			FPackageName::EFlexNameType::ObjectPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(TEXT("/Engine/Foo.Bar:Baz"), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			TEXT("Bar"), TEXT("Baz"), FStringView(),
			FPackageName::EFlexNameType::ObjectPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(TEXT("/Engine/Foo.Spaces Ignored"), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			TEXT("Spaces Ignored"), FStringView(), FStringView(),
			FPackageName::EFlexNameType::ObjectPath, FPackageName::EErrorCode::PackageNameUnknown);
		TestConvert(TEXT("/Engine/Foo.Spaces Ignored:For Spaces"), true, EngineLocalPath(TEXT("Foo")), TEXT("/Engine/Foo"),
			TEXT("Spaces Ignored"), TEXT("For Spaces"), FStringView(),
			FPackageName::EFlexNameType::ObjectPath, FPackageName::EErrorCode::PackageNameUnknown);

		TestConvert(TEXT("/Engine/Package Spaces"), false, FStringView(), FStringView(),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::Invalid, FPackageName::EErrorCode::PackageNameSpacesNotAllowed);
		TestConvert(TEXT("/Engine/PackageNameWithInvalidChar?"), false, FStringView(), FStringView(),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::Invalid, FPackageName::EErrorCode::PackageNameContainsInvalidCharacters);
		TestConvert(TEXT("Texture2D /Engine/Foo"), false, FStringView(), FStringView(),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::Invalid, FPackageName::EErrorCode::PackageNameSpacesNotAllowed);
		TestConvert(TEXT("/FPackageNameTests_DNE/Foo"), false, FStringView(), FStringView(),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::Invalid, FPackageName::EErrorCode::PackageNamePathNotMounted);
		TestConvert(TEXT("../../../FPackageNameTests_DNE/Foo"), false, FStringView(), FStringView(),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::Invalid, FPackageName::EErrorCode::PackageNamePathNotMounted);
		TestConvert(TEXT(""), false, FStringView(), FStringView(),
			FStringView(), FStringView(), FStringView(),
			FPackageName::EFlexNameType::Invalid, FPackageName::EErrorCode::PackageNameEmptyPath);
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
#undef LOCTEXT_NAMESPACE // "PackageNames"
