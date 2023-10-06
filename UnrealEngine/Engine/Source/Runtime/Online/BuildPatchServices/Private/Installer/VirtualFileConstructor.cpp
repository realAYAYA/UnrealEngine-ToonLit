// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/VirtualFileConstructor.h"

#include "BuildPatchFileConstructor.h"
#include "Data/ManifestData.h"
#include "IBuildManifestSet.h"
#include "Installer/ChunkSource.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/InstallerError.h"
#include "Interfaces/IBuildInstaller.h"
#include "Logging/LogMacros.h"
#include "VirtualFileCache.h"

// TODO: Move EBuildPatchInstallError into it's own header header (replacing Interfaces/IBuildInstaller.h include)
// TODO: Move IFileConstructorStat into it's own header (replacing BuildPatchFileConstructor.h include)

DEFINE_LOG_CATEGORY_STATIC(LogVirtualFileConstructor, Log, All);

namespace BuildPatchServices
{
	FVirtualFileConstructor::FVirtualFileConstructor(FVirtualFileConstructorConfiguration InConfiguration, FVirtualFileConstructorDependencies InDependencies)
		: Configuration(MoveTemp(InConfiguration))
		, Dependencies(MoveTemp(InDependencies))
	{
	}

	FVirtualFileConstructor::~FVirtualFileConstructor()
	{
	}

	bool FVirtualFileConstructor::Run()
	{
		// TODO: Implement abort / cancel
		const bool bShouldAbort = false;

		bool bFileSuccess = true;
		for (auto FileIt = Configuration.FilesToConstruct.CreateConstIterator(); FileIt && bFileSuccess; ++FileIt)
		{
			FSHA1 HashState;
			FSHAHash HashValue;
			TArray<uint8> FileBytes;
			const FString& FileToConstruct = *FileIt;
			const FFileManifest* FileManifest = Dependencies.ManifestSet->GetNewFileManifest(FileToConstruct);
			bFileSuccess = FileManifest != nullptr;
			if (bFileSuccess)
			{
				FileBytes.Reserve(FileManifest->FileSize);
				Dependencies.FileConstructorStat->OnFileStarted(FileToConstruct, FileManifest->FileSize);
				Dependencies.FileConstructorStat->OnFileProgress(FileToConstruct, 0);
				for (int32 ChunkPartIdx = 0; ChunkPartIdx < FileManifest->ChunkParts.Num() && bFileSuccess && !bShouldAbort; ++ChunkPartIdx)
				{
					const FChunkPart& ChunkPart = FileManifest->ChunkParts[ChunkPartIdx];
					Dependencies.FileConstructorStat->OnChunkGet(ChunkPart.Guid);
					IChunkDataAccess* ChunkDataAccess = Dependencies.ChunkSource->Get(ChunkPart.Guid);
					bFileSuccess = ChunkDataAccess != nullptr;
					if (bFileSuccess)
					{
						uint8* Data;
						ChunkDataAccess->GetDataLock(&Data, nullptr);
						uint8* DataStart = &Data[ChunkPart.Offset];
						HashState.Update(DataStart, ChunkPart.Size);
						FileBytes.Append(DataStart, ChunkPart.Size);
						ChunkDataAccess->ReleaseDataLock();
						Dependencies.FileConstructorStat->OnFileProgress(FileToConstruct, FileBytes.Num());
						bFileSuccess = Dependencies.ChunkReferenceTracker->PopReference(ChunkPart.Guid);
						if (!bFileSuccess)
						{
							UE_LOG(LogVirtualFileConstructor, Error, TEXT("Failed %s due to untracked chunk %s"), *FileToConstruct, *ChunkPart.Guid.ToString());
							Dependencies.InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::TrackingError);
						}
					}
					else
					{
						UE_LOG(LogVirtualFileConstructor, Error, TEXT("Failed %s due to missing chunk %s"), *FileToConstruct, *ChunkPart.Guid.ToString());
						Dependencies.InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingChunkData);
					}
				}
				if (bFileSuccess)
				{
					HashState.Final();
					HashState.GetHash(HashValue.Hash);
					bFileSuccess = HashValue == FileManifest->FileHash;
					if (bFileSuccess)
					{
						Dependencies.VirtualFileCache->WriteData(HashValue, FileBytes.GetData(), FileBytes.Num());
					}
					else
					{
						UE_LOG(LogVirtualFileConstructor, Error, TEXT("Verify failed after constructing %s"), *FileToConstruct);
					}
				}
				Dependencies.FileConstructorStat->OnFileCompleted(FileToConstruct, bFileSuccess);
			}
			else
			{
				UE_LOG(LogVirtualFileConstructor, Error, TEXT("Missing file manifest for %s"), *FileToConstruct);
				Dependencies.InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
			}
		}
		return bFileSuccess;
	}

	FVirtualFileConstructor* FVirtualFileConstructorFactory::Create(FVirtualFileConstructorConfiguration Configuration, FVirtualFileConstructorDependencies Dependencies)
	{
		return new FVirtualFileConstructor(MoveTemp(Configuration), MoveTemp(Dependencies));
	}
}
