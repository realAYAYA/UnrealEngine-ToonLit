// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hash/Blake3.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "Memory/CompositeBuffer.h"
#include "Memory/SharedBuffer.h"
#include "blake3.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static_assert(sizeof(FBlake3) == sizeof(blake3_hasher), "Adjust the allocation in FBlake3 to match blake3_hasher");

void FBlake3::Reset()
{
	blake3_hasher& Hasher = reinterpret_cast<blake3_hasher&>(HasherBytes);
	blake3_hasher_init(&Hasher);
}

void FBlake3::Update(FMemoryView View)
{
	blake3_hasher& Hasher = reinterpret_cast<blake3_hasher&>(HasherBytes);
	blake3_hasher_update(&Hasher, View.GetData(), View.GetSize());
}

void FBlake3::Update(const void* Data, uint64 Size)
{
	blake3_hasher& Hasher = reinterpret_cast<blake3_hasher&>(HasherBytes);
	blake3_hasher_update(&Hasher, Data, Size);
}

void FBlake3::Update(const FCompositeBuffer& Buffer)
{
	blake3_hasher& Hasher = reinterpret_cast<blake3_hasher&>(HasherBytes);
	for (const FSharedBuffer& Segment : Buffer.GetSegments())
	{
		blake3_hasher_update(&Hasher, Segment.GetData(), Segment.GetSize());
	}
}

FBlake3Hash FBlake3::Finalize() const
{
	FBlake3Hash Hash;
	FBlake3Hash::ByteArray& Output = Hash.GetBytes();
	static_assert(sizeof(decltype(Output)) == BLAKE3_OUT_LEN, "Mismatch in BLAKE3 hash size.");
	const blake3_hasher& Hasher = reinterpret_cast<const blake3_hasher&>(HasherBytes);
	blake3_hasher_finalize(&Hasher, Output, BLAKE3_OUT_LEN);
	return Hash;
}

FBlake3Hash FBlake3::HashBuffer(FMemoryView View)
{
	FBlake3 Hash;
	Hash.Update(View.GetData(), View.GetSize());
	return Hash.Finalize();
}

FBlake3Hash FBlake3::HashBuffer(const void* Data, uint64 Size)
{
	FBlake3 Hash;
	Hash.Update(Data, Size);
	return Hash.Finalize();
}

FBlake3Hash FBlake3::HashBuffer(const FCompositeBuffer& Buffer)
{
	FBlake3 Hash;
	Hash.Update(Buffer);
	return Hash.Finalize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString LexToString(const FBlake3Hash& Hash)
{
	FString Output;
	TArray<TCHAR, FString::AllocatorType>& CharArray = Output.GetCharArray();
	CharArray.AddUninitialized(sizeof(FBlake3Hash::ByteArray) * 2 + 1);
	UE::String::BytesToHexLower(Hash.GetBytes(), CharArray.GetData());
	CharArray.Last() = TCHAR('\0');
	return Output;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
