// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/ChunkReferenceTracker.h"
#include "Templates/Greater.h"
#include "Algo/Sort.h"
#include "Misc/ScopeLock.h"
#include "IBuildManifestSet.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkReferenceTracker, Warning, All);
DEFINE_LOG_CATEGORY(LogChunkReferenceTracker);

namespace BuildPatchServices
{
	class FChunkReferenceTracker : public IChunkReferenceTracker
	{
	public:
		FChunkReferenceTracker(const IBuildManifestSet* ManifestSet, const TSet<FString>& FilesToConstruct);
		FChunkReferenceTracker(TArray<FGuid> CustomUseStack);

		~FChunkReferenceTracker();

		// IChunkReferenceTracker interface begin.
		virtual TSet<FGuid> GetReferencedChunks() const override;
		virtual int32 GetReferenceCount(const FGuid& ChunkId) const override;
		virtual void SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const override;
		virtual TArray<FGuid> GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const override;
		virtual TArray<FGuid> SelectFromNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const override;
		virtual bool PopReference(const FGuid& ChunkId) override;
		// IChunkReferenceTracker interface end.

	private:
		TMap<FGuid, FThreadSafeCounter> ReferenceCount;
		TArray<FGuid> UseStack;
		mutable FCriticalSection UseStackCs;
	};

	FChunkReferenceTracker::FChunkReferenceTracker(const IBuildManifestSet* ManifestSet, const TSet<FString>& FilesToConstruct)
		: ReferenceCount()
		, UseStack()
		, UseStackCs()
	{
		// Create our full list of chunks, including dupe references, and track the reference count of each chunk.
		for (const FString& File : FilesToConstruct)
		{
			const FFileManifest* NewFileManifest = ManifestSet->GetNewFileManifest(File);
			if (NewFileManifest != nullptr)
			{
				for (const FChunkPart& ChunkPart : NewFileManifest->ChunkParts)
				{
					ReferenceCount.FindOrAdd(ChunkPart.Guid).Increment();
					UseStack.Add(ChunkPart.Guid);
				}
			}
		}
		// Reverse the order of UseStack so it can be used as a stack.
		Algo::Reverse(UseStack);
		UE_LOG(LogChunkReferenceTracker, VeryVerbose, TEXT("Created. Total references:%d. Unique chunks:%d"), UseStack.Num(), ReferenceCount.Num());
	}

	FChunkReferenceTracker::FChunkReferenceTracker(TArray<FGuid> CustomChunkReferences)
		: ReferenceCount()
		, UseStack(MoveTemp(CustomChunkReferences))
		, UseStackCs()
	{
		ReferenceCount.Reserve(UseStack.Num());
		for (const FGuid& Chunk : UseStack)
		{
			ReferenceCount.FindOrAdd(Chunk).Increment();
		}
		// Reverse the order of UseStack so it can be used as a stack.
		Algo::Reverse(UseStack);
		UE_LOG(LogChunkReferenceTracker, VeryVerbose, TEXT("Created. Total references:%d. Unique chunks:%d"), UseStack.Num(), ReferenceCount.Num());
	}

	FChunkReferenceTracker::~FChunkReferenceTracker()
	{
	}

	TSet<FGuid> FChunkReferenceTracker::GetReferencedChunks() const
	{
		TSet<FGuid> ReferencedChunks;
		for (const TPair<FGuid, FThreadSafeCounter>& Pair : ReferenceCount)
		{
			if (Pair.Value.GetValue() > 0)
			{
				ReferencedChunks.Add(Pair.Key);
			}
		}
		return ReferencedChunks;
	}

	int32 FChunkReferenceTracker::GetReferenceCount(const FGuid& ChunkId) const
	{
		return ReferenceCount.Contains(ChunkId) ? ReferenceCount[ChunkId].GetValue() : 0;
	}

	void FChunkReferenceTracker::SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const
	{
		// Thread lock to protect access to UseStack.
		FScopeLock ThreadLock(&UseStackCs);
		struct FIndexCache
		{
			FIndexCache(const TArray<FGuid>& InArray)
				: Array(InArray)
			{}

			int32 GetIndex(const FGuid& Id)
			{
				if (!IndexCache.Contains(Id))
				{
					IndexCache.Add(Id, Array.FindLast(Id));
				}
				return IndexCache[Id];
			}

			const TArray<FGuid>& Array;
			TMap<FGuid, int32> IndexCache;
		};
		FIndexCache ChunkUseIndexes(UseStack);
		switch (Direction)
		{
			case ESortDirection::Ascending:
				Algo::SortBy(ChunkList, [&ChunkUseIndexes](const FGuid& Id) { return ChunkUseIndexes.GetIndex(Id); }, TGreater<int32>());
				break;
			case ESortDirection::Descending:
				Algo::SortBy(ChunkList, [&ChunkUseIndexes](const FGuid& Id) { return ChunkUseIndexes.GetIndex(Id); }, TLess<int32>());
				break;
		}
	}

	TArray<FGuid> FChunkReferenceTracker::GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const
	{
		// Thread lock to protect access to UseStack.
		FScopeLock ThreadLock(&UseStackCs);
		TSet<FGuid> AddedIds;
		TArray<FGuid> NextReferences;
		for (int32 UseStackIdx = UseStack.Num() - 1; UseStackIdx >= 0 && Count > NextReferences.Num(); --UseStackIdx)
		{
			const FGuid& UseId = UseStack[UseStackIdx];
			if (AddedIds.Contains(UseId) == false && SelectPredicate(UseId))
			{
				AddedIds.Add(UseId);
				NextReferences.Add(UseId);
			}
		}
		return NextReferences;
	}

	TArray<FGuid> FChunkReferenceTracker::SelectFromNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const
	{
		// Thread lock to protect access to UseStack.
		FScopeLock ThreadLock(&UseStackCs);
		TSet<FGuid> AddedIds;
		TArray<FGuid> NextReferences;
		for (int32 UseStackIdx = UseStack.Num() - 1; UseStackIdx >= 0 && Count > 0; --UseStackIdx)
		{
			const FGuid& UseId = UseStack[UseStackIdx];
			if (AddedIds.Contains(UseId) == false)
			{
				--Count;
				if (SelectPredicate(UseId))
				{
					AddedIds.Add(UseId);
					NextReferences.Add(UseId);
				}
			}
		}
		return NextReferences;
	}

	bool FChunkReferenceTracker::PopReference(const FGuid& ChunkId)
	{
		// Thread lock to protect access to UseStack.
		FScopeLock ThreadLock(&UseStackCs);
		if (UseStack.Last() == ChunkId)
		{
			ReferenceCount[ChunkId].Decrement();
			UseStack.Pop();
			return true;
		}
		return false;
	}

	IChunkReferenceTracker* FChunkReferenceTrackerFactory::Create(const IBuildManifestSet* ManifestSet, const TSet<FString>& FilesToConstruct)
	{
		return new FChunkReferenceTracker(ManifestSet, FilesToConstruct);
	}

	IChunkReferenceTracker* FChunkReferenceTrackerFactory::Create(TArray<FGuid> CustomChunkReferences)
	{
		return new FChunkReferenceTracker(MoveTemp(CustomChunkReferences));
	}
}