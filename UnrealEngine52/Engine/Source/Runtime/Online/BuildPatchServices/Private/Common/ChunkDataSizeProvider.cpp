// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/ChunkDataSizeProvider.h"
#include "Misc/Paths.h"
#include "BuildPatchUtil.h"

namespace BuildPatchServices
{
	class FChunkDataSizeProvider
		: public IChunkDataSizeProvider
	{
	public:
		// Begin IDataSizeProvider
		virtual uint64 GetDownloadSize(const FString& Identifier) const override
		{
			checkSlow(IsInGameThread());

			FGuid ChunkGUID;
			bool bFoundGUID = FBuildPatchUtils::GetGUIDFromFilename(Identifier, ChunkGUID);

			uint64 DownloadSize = INDEX_NONE;
			if (bFoundGUID)
			{
				for (int32 i = Manifests.Num() - 1; i >= 0; --i)
				{
					const FBuildPatchAppManifestPtr& Manifest = Manifests[i];
					const BuildPatchServices::FChunkInfo* ChunkInfo = Manifest->GetChunkInfo(ChunkGUID);
					if (ChunkInfo)
					{
						DownloadSize = ChunkInfo->FileSize;
						break;
					}
				}
			}
			return DownloadSize;
		}
		// End IDataSizeProvider

		// Begin IChunkDataSizeProvider
		virtual void AddManifestData(FBuildPatchAppManifestPtr Manifest) override
		{
			check(IsInGameThread());

			if (Manifest)
			{
				int32 Index = Manifests.Find(Manifest);
				if (Index != INDEX_NONE)
				{
					Manifests.RemoveAt(Index, 1, false);
				}

				Manifests.Add(MoveTemp(Manifest));  // Last manifest added always goes on the end
			}
		}
		// End IChunkDataSizeProvider

	private:
		TArray<FBuildPatchAppManifestPtr> Manifests;
	};
	
	IChunkDataSizeProvider* FChunkDataSizeProviderFactory::Create()
	{
		return new FChunkDataSizeProvider();
	}
}
