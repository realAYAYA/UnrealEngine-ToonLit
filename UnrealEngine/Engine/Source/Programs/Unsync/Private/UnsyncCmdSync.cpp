// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdSync.h"
#include "UnsyncFile.h"
#include "UnsyncProxy.h"

namespace unsync {

int32  // TODO: return a TResult
CmdSync(const FCmdSyncOptions& Options)
{
	auto ResolvePath = [&Options](const FPath& Path) -> FPath
	{
		return Options.Filter ? Options.Filter->Resolve(Path) : Path;
	};

	std::error_code ErrorCode	   = {};
	FPath			ResolvedSource = ResolvePath(Options.Source);

	if (Options.Source == ResolvedSource)
	{
		UNSYNC_VERBOSE(L"Sync source: '%ls'", Options.Source.wstring().c_str());
	}
	else
	{
		UNSYNC_VERBOSE(L"Sync source: '%ls' ('%ls')", Options.Source.wstring().c_str(), ResolvedSource.wstring().c_str());
	}

	if (!Options.Filter->SyncIncludedWords.empty())
	{
		UNSYNC_VERBOSE(L"Include filter: ");
		for( const std::wstring& include : Options.Filter->SyncIncludedWords)
		{ 
			UNSYNC_VERBOSE(L"\t%s", include.c_str());
		}
	}
	if(!Options.Filter->SyncExcludedWords.empty())
	{
		UNSYNC_VERBOSE(L"Exclude filter: ");
		for( const std::wstring& exclude : Options.Filter->SyncExcludedWords)
		{ 
			UNSYNC_VERBOSE(L"\t%s", exclude.c_str());
		}
	}

	const bool bSourcePathExists	 = PathExists(ResolvedSource, ErrorCode);
	const bool bSourceIsDirectory	 = bSourcePathExists && unsync::IsDirectory(ResolvedSource);
	const bool bSourceIsManifestHash = !bSourcePathExists && LooksLikeHash160(Options.Source.native());

	bool bSourceFileSystemRequired = !bSourceIsManifestHash;

	if (!Options.Overlays.empty())
	{
		for (const FPath& Entry : Options.Overlays)
		{
			FPath ResolvedEntry = ResolvePath(Entry);
			if (ResolvedEntry == Entry)
			{
				UNSYNC_VERBOSE(L"Sync overlay: '%ls'", ResolvedEntry.wstring().c_str());
			}
			else
			{
				UNSYNC_VERBOSE(L"Sync overlay: '%ls' ('%ls')", Entry.wstring().c_str(), ResolvedEntry.wstring().c_str());
			}
		}

		bSourceFileSystemRequired = true;

		if (!bSourcePathExists || !bSourceIsDirectory)
		{
			UNSYNC_ERROR(L"Sync overlay option requires sync source to be a directory that exists on disk.");
			return 1;
		}

		if (bSourceIsManifestHash)
		{
			UNSYNC_ERROR(L"Sync overlay option is not compatible with manifest override.");
			return 1;
		}
	}

	UNSYNC_VERBOSE(L"Sync target: '%ls'", Options.Target.wstring().c_str());

	if (!Options.SourceManifestOverride.empty())
	{
		UNSYNC_VERBOSE(L"Manifest override: %ls", Options.SourceManifestOverride.wstring().c_str());

		if (Options.Remote.IsValid() && !Options.bFullSourceScan)
		{
			bSourceFileSystemRequired = false;
		}
	}

	UNSYNC_VERBOSE(L"Source directory access is %ls", bSourceFileSystemRequired ? L"required" : L"NOT required");

	if (bSourcePathExists || !bSourceFileSystemRequired)
	{
		if (!bSourceFileSystemRequired || bSourceIsDirectory)
		{
			if (bSourceIsDirectory)
			{
				UNSYNC_VERBOSE(L"'%ls' is a directory", Options.Source.wstring().c_str());
			}
			else
			{
				UNSYNC_VERBOSE(L"Assuming '%ls' is a directory", Options.Source.wstring().c_str());
			}

			FSyncDirectoryOptions SyncOptions;

			if (bSourceIsManifestHash || !bSourceFileSystemRequired)
			{
				SyncOptions.SourceType = ESyncSourceType::Server;
			}
			else
			{
				SyncOptions.SourceType = ESyncSourceType::FileSystem;
			}

			SyncOptions.Source				   = Options.Source;
			SyncOptions.Base				   = Options.Target;  // read base data from existing target
			SyncOptions.Target				   = Options.Target;
			SyncOptions.Overlays			   = Options.Overlays;
			SyncOptions.SourceManifestOverride = Options.SourceManifestOverride;
			SyncOptions.Remote				   = &Options.Remote;
			SyncOptions.SyncFilter			   = Options.Filter;
			SyncOptions.bCleanup			   = Options.bCleanup;
			SyncOptions.bValidateSourceFiles   = Options.bFullSourceScan;
			SyncOptions.bFullDifference		   = Options.bFullDifference;
			SyncOptions.bValidateTargetFiles   = Options.bValidateTargetFiles;

			return SyncDirectory(SyncOptions) ? 0 : 1;
		}
		else
		{
			UNSYNC_VERBOSE(L"'%ls' is a file", Options.Source.wstring().c_str());

			FSyncFileOptions SyncFileOptions;
			SyncFileOptions.Algorithm			 = Options.Algorithm;
			SyncFileOptions.BlockSize			 = Options.BlockSize;
			SyncFileOptions.bValidateTargetFiles = Options.bValidateTargetFiles;

			return SyncFile(Options.Source, Options.Target, Options.Target, SyncFileOptions).Succeeded() ? 0 : 1;
		}
	}
	else
	{
		if (ErrorCode)
		{
			UNSYNC_ERROR(L"System error code %d: %hs", ErrorCode.value(), ErrorCode.message().c_str());
			return ErrorCode.value();
		}
		else
		{
			UNSYNC_ERROR(L"Source path does not exist");
			return 1;
		}
	}

	return 0;
}

}  // namespace unsync
