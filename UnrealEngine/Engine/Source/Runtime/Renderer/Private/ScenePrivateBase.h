// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

typedef TBitArray<SceneRenderingBitArrayAllocator> FSceneBitArray;
typedef TConstSetBitIterator<SceneRenderingBitArrayAllocator> FSceneSetBitIterator;
typedef TConstDualSetBitIterator<SceneRenderingBitArrayAllocator,SceneRenderingBitArrayAllocator> FSceneDualSetBitIterator;

// Forward declarations.
class FScene;

class FOcclusionQueryHelpers
{
public:

	enum
	{
		MaxBufferedOcclusionFrames = 4
	};

	// get the system-wide number of frames of buffered occlusion queries.
	static int32 GetNumBufferedFrames(ERHIFeatureLevel::Type FeatureLevel);

	// get the index of the oldest query based on the current frame and number of buffered frames.
	static uint32 GetQueryLookupIndex(int32 CurrentFrame, int32 NumBufferedFrames)
	{
		// queries are currently always requested earlier in the frame than they are issued.
		// thus we can always overwrite the oldest query with the current one as we never need them
		// to coexist.  This saves us a buffer entry.
		const uint32 QueryIndex = CurrentFrame % NumBufferedFrames;
		return QueryIndex;
	}

	// get the index of the query to overwrite for new queries.
	static uint32 GetQueryIssueIndex(int32 CurrentFrame, int32 NumBufferedFrames)
	{
		// queries are currently always requested earlier in the frame than they are issued.
		// thus we can always overwrite the oldest query with the current one as we never need them
		// to coexist.  This saves us a buffer entry.
		const uint32 QueryIndex = CurrentFrame % NumBufferedFrames;
		return QueryIndex;
	}
};

/** A simple chunked array representation for scene primitives data arrays. */
template <typename T>
class TScenePrimitiveArray
{
	static const int32 NumElementsPerChunk = 1024;
public:
	TScenePrimitiveArray() = default;

	T& Add(const T& Element)
	{
		return *(new (&AddUninitialized()) T(Element));
	}

	T& AddUninitialized()
	{
		if (NumElements % NumElementsPerChunk == 0)
		{
			ChunkType* Chunk = new ChunkType;
			Chunk->Reserve(NumElementsPerChunk);
			Chunks.Emplace(Chunk);
		}

		NumElements++;
		ChunkType& Chunk = *Chunks.Last();
		Chunk.AddUninitialized();
		return Chunk.Last();
	}

	void Remove(uint32 Count, EAllowShrinking AllowShrinking)
	{
		check(Count <= NumElements);
		const uint32 NumElementsNew = NumElements - Count;
		while (NumElements != NumElementsNew)
		{
			--NumElements;

			ChunkType& Chunk = *Chunks.Last();
			Chunk.Pop(EAllowShrinking::No);

			if (Chunk.IsEmpty())
			{
				Chunks.Pop(EAllowShrinking::No);
			}
		}
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("Remove")
	FORCEINLINE void Remove(uint32 Count, bool bAllowShrinking)
	{
		Remove(Count, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	void Reserve(int32 Count)
	{
		Chunks.Reserve(NumChunks(Count));
	}

	T& Get(int32 ElementIndex)
	{
		const uint32 ChunkIndex        = ElementIndex / NumElementsPerChunk;
		const uint32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return (*Chunks[ChunkIndex])[ChunkElementIndex];
	}

	const T& Get(int32 ElementIndex) const
	{
		const uint32 ChunkIndex        = ElementIndex / NumElementsPerChunk;
		const uint32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return (*Chunks[ChunkIndex])[ChunkElementIndex];
	}

	FORCEINLINE T& operator[] (int32 Index) { return Get(Index); }
	FORCEINLINE const T& operator[] (int32 Index) const { return Get(Index); }

	bool IsValidIndex(int32 Index) const { return static_cast<uint32>(Index) < NumElements; }

	int32 Num() const { return NumElements; }

private:
	static constexpr uint32 NumChunks(uint32 NumElements)
	{
		return (NumElements + NumElementsPerChunk - 1u) / NumElementsPerChunk;
	}

	using ChunkType = TArray<T>;
	TArray<TUniquePtr<ChunkType>> Chunks;
	uint32 NumElements = 0;
};