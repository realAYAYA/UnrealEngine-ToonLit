// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hash/xxhash.h"

#include "Containers/ContainersFwd.h"
#include "Memory/CompositeBuffer.h"
#include "Memory/MemoryView.h"
#include "Memory/SharedBuffer.h"

THIRD_PARTY_INCLUDES_START
#define XXH_INLINE_ALL
#include "ThirdParty/xxhash/xxhash.h"
THIRD_PARTY_INCLUDES_END

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
