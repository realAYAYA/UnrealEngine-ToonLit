// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hash/xxhash.h"

#include "Async/ParallelFor.h"
#include "Containers/ContainersFwd.h"
#include "Containers/StaticArray.h"
#include "Memory/CompositeBuffer.h"
#include "Memory/MemoryView.h"
#include "Memory/SharedBuffer.h"

THIRD_PARTY_INCLUDES_START
#define XXH_INLINE_ALL
#include <xxhash.h>
THIRD_PARTY_INCLUDES_END

template <typename HashType>
static HashType PrivateHashBufferChunked(const FMemoryView View, const uint64 ChunkSize)
{
	if (View.GetSize() <= ChunkSize || ChunkSize == 0)
	{
		return HashType::HashBuffer(View);
	}

	// Due to a limitation of ParallelFor, input size is limited to MAX_int32 * ChunkSize.
	const int32 ChunkCount = IntCastChecked<int32>((View.GetSize() + ChunkSize - 1) / ChunkSize);

	TArray<TStaticArray<uint8, sizeof(HashType)>, TInlineAllocator<64>> ChunkHashes;
	ChunkHashes.AddZeroed(ChunkCount + 1);

	ParallelFor(TEXT("XxHash.PF"), ChunkCount, 1, [View, ChunkSize, &ChunkHashes](const int32 Index)
	{
		FMemoryView ChunkView = View.Mid(Index * ChunkSize, ChunkSize);
		HashType::HashBuffer(ChunkView).ToByteArray((uint8(&)[sizeof(HashType)])ChunkHashes[Index]);
	});

	static_assert(sizeof(uint64) <= sizeof(HashType));
	FPlatformMemory::WriteUnaligned(&ChunkHashes.Last(), INTEL_ORDER64(View.GetSize()));

	return HashType::HashBuffer(ChunkHashes.GetData(), ChunkCount * sizeof(HashType) + sizeof(uint64));
}

FXxHash64 FXxHash64::HashBuffer(FMemoryView View)
{
	return {XXH3_64bits(View.GetData(), View.GetSize())};
}

FXxHash64 FXxHash64::HashBuffer(const void* Data, uint64 Size)
{
	return {XXH3_64bits(Data, Size)};
}

FXxHash64 FXxHash64::HashBuffer(const FCompositeBuffer& Buffer)
{
	FXxHash64Builder Builder;
	Builder.Update(Buffer);
	return Builder.Finalize();
}

FXxHash64 FXxHash64::HashBufferChunked(const FMemoryView View, const uint64 ChunkSize)
{
	return PrivateHashBufferChunked<FXxHash64>(View, ChunkSize);
}

FXxHash64 FXxHash64::HashBufferChunked(const void* Data, uint64 Size, uint64 ChunkSize)
{
	return PrivateHashBufferChunked<FXxHash64>(FMemoryView(Data, Size), ChunkSize);
}

FXxHash128 FXxHash128::HashBuffer(FMemoryView View)
{
	const XXH128_hash_t Value = XXH3_128bits(View.GetData(), View.GetSize());
	return {Value.low64, Value.high64};
}

FXxHash128 FXxHash128::HashBuffer(const void* Data, uint64 Size)
{
	const XXH128_hash_t Value = XXH3_128bits(Data, Size);
	return {Value.low64, Value.high64};
}

FXxHash128 FXxHash128::HashBufferChunked(const FMemoryView View, const uint64 ChunkSize)
{
	return PrivateHashBufferChunked<FXxHash128>(View, ChunkSize);
}

FXxHash128 FXxHash128::HashBufferChunked(const void* Data, uint64 Size, uint64 ChunkSize)
{
	return PrivateHashBufferChunked<FXxHash128>(FMemoryView(Data, Size), ChunkSize);
}

FXxHash128 FXxHash128::HashBuffer(const FCompositeBuffer& Buffer)
{
	FXxHash128Builder Builder;
	Builder.Update(Buffer);
	return Builder.Finalize();
}

void FXxHash64Builder::Reset()
{
	static_assert(sizeof(StateBytes) == sizeof(XXH3_state_t), "Adjust the allocation in FXxHash64Builder to match XXH3_state_t");
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	XXH3_64bits_reset(&State);
}

void FXxHash64Builder::Update(FMemoryView View)
{
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	XXH3_64bits_update(&State, View.GetData(), View.GetSize());
}

void FXxHash64Builder::Update(const void* Data, uint64 Size)
{
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	XXH3_64bits_update(&State, Data, Size);
}

void FXxHash64Builder::Update(const FCompositeBuffer& Buffer)
{
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	for (const FSharedBuffer& Segment : Buffer.GetSegments())
	{
		XXH3_64bits_update(&State, Segment.GetData(), Segment.GetSize());
	}
}

FXxHash64 FXxHash64Builder::Finalize() const
{
	const XXH3_state_t& State = reinterpret_cast<const XXH3_state_t&>(StateBytes);
	const XXH64_hash_t Hash = XXH3_64bits_digest(&State);
	return {Hash};
}

void FXxHash128Builder::Reset()
{
	static_assert(sizeof(StateBytes) == sizeof(XXH3_state_t), "Adjust the allocation in FXxHash128Builder to match XXH3_state_t");
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	XXH3_128bits_reset(&State);
}

void FXxHash128Builder::Update(FMemoryView View)
{
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	XXH3_128bits_update(&State, View.GetData(), View.GetSize());
}

void FXxHash128Builder::Update(const void* Data, uint64 Size)
{
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	XXH3_128bits_update(&State, Data, Size);
}

void FXxHash128Builder::Update(const FCompositeBuffer& Buffer)
{
	XXH3_state_t& State = reinterpret_cast<XXH3_state_t&>(StateBytes);
	for (const FSharedBuffer& Segment : Buffer.GetSegments())
	{
		XXH3_128bits_update(&State, Segment.GetData(), Segment.GetSize());
	}
}

FXxHash128 FXxHash128Builder::Finalize() const
{
	const XXH3_state_t& State = reinterpret_cast<const XXH3_state_t&>(StateBytes);
	const XXH128_hash_t Hash = XXH3_128bits_digest(&State);
	return {Hash.low64, Hash.high64};
}
