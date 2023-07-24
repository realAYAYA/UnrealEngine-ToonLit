// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdHash.h"
#include "UnsyncCore.h"
#include "UnsyncFile.h"
#include "UnsyncSerialization.h"

namespace unsync {

int32  // TODO: return a TResult
CmdHash(const FCmdHashOptions& Options)
{
	if (unsync::IsDirectory(Options.Input))
	{
		UNSYNC_VERBOSE(L"'%ls' is a directory", Options.Input.wstring().c_str());

		FPath InputRoot	   = Options.Input;
		FPath ManifestRoot = InputRoot / ".unsync";
		FPath DirectoryManifestPath;

		if (Options.Output.empty())
		{
			DirectoryManifestPath = ManifestRoot / "manifest.bin";

			bool bDirectoryExists = (PathExists(ManifestRoot) && IsDirectory(ManifestRoot)) || (GDryRun || CreateDirectories(ManifestRoot));

			if (!bDirectoryExists && !GDryRun)
			{
				UNSYNC_ERROR(L"Failed to initialize manifest directory '%ls'", ManifestRoot.wstring().c_str());
				return 1;
			}
		}
		else
		{
			DirectoryManifestPath = Options.Output;
		}

		FDirectoryManifest DirectoryManifest;
		if (Options.bForce)
		{
			DirectoryManifest = CreateDirectoryManifest(Options.Input, Options.BlockSize, Options.Algorithm);
		}
		else if (Options.bIncremental)
		{
			UNSYNC_VERBOSE(L"Performing incremental directory manifest generation");
			DirectoryManifest = CreateDirectoryManifestIncremental(Options.Input, Options.BlockSize, Options.Algorithm);
		}
		else
		{
			LoadOrCreateDirectoryManifest(DirectoryManifest, Options.Input, Options.BlockSize, Options.Algorithm);
		}

		if (!GDryRun)
		{
			SaveDirectoryManifest(DirectoryManifest, DirectoryManifestPath);
		}
	}
	else
	{
		UNSYNC_VERBOSE(L"'%ls' is a file", Options.Input.wstring().c_str());

		NativeFile OverlappedFile(Options.Input);
		if (OverlappedFile.IsValid())
		{
			UNSYNC_VERBOSE(L"Computing blocks for '%ls' (%.2f MB)", Options.Input.wstring().c_str(), SizeMb(OverlappedFile.GetSize()));
			FGenericBlockArray GenericBlocks = ComputeBlocks(OverlappedFile, Options.BlockSize, Options.Algorithm);

			FPath OutputFilename = Options.Output;
			if (OutputFilename.empty())
			{
				OutputFilename = Options.Input.wstring() + std::wstring(L".unsync");
			}

			if (!GDryRun)
			{
				UNSYNC_VERBOSE(L"Saving blocks to '%ls'", OutputFilename.wstring().c_str());
				std::vector<FBlock128> Blocks128;
				Blocks128.reserve(GenericBlocks.size());  // #wip-widehash
				for (const auto& It : GenericBlocks)
				{
					FBlock128 Block;
					Block.HashStrong = It.HashStrong.ToHash128();
					Block.HashWeak	 = It.HashWeak;
					Block.Offset	 = It.Offset;
					Block.Size		 = It.Size;
					Blocks128.push_back(Block);
				}
				return SaveBlocks(Blocks128, Options.BlockSize, OutputFilename) ? 0 : 1;
			}
		}
		else
		{
			UNSYNC_ERROR(L"Failed to open file '%ls'", Options.Input.wstring().c_str());
			return 1;
		}
	}

	return 0;
}

}  // namespace unsync
