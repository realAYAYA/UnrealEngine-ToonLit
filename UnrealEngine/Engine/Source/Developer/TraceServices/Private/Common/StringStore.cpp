// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/StringStore.h"
#include "Misc/ScopeLock.h"


namespace TraceServices
{

FStringStore::FStringStore(FSlabAllocator& InAllocator)
	: Allocator(InAllocator)
{

}

const TCHAR* FStringStore::Store(const TCHAR* String)
{
	return Store(FStringView(String));
}

const TCHAR* FStringStore::Store(const FStringView& String)
{
	FScopeLock _(&Cs);
	uint32 Hash = GetTypeHash(String);
	const TCHAR** AlreadyStored = StoredStrings.Find(Hash);
	if (AlreadyStored && !String.Compare(FStringView(*AlreadyStored)))
	{
		return *AlreadyStored;
	}
	
	int32 StringLength = String.Len() + 1;
	if (BufferLeft < StringLength)
	{
		BufferPtr = reinterpret_cast<TCHAR*>(Allocator.Allocate(BlockSize * sizeof(TCHAR)));
		++BlockCount;
		BufferLeft = BlockSize;
	}
	const TCHAR* Stored = BufferPtr;
	memcpy(BufferPtr, String.GetData(), (StringLength - 1) * sizeof(TCHAR));
	BufferPtr[StringLength - 1] = TEXT('\0');
	BufferLeft -= StringLength;
	BufferPtr += StringLength;
	if (!AlreadyStored)
	{
		StoredStrings.Add(Hash, Stored);
	}
	return Stored;
}


} // namespace TraceServices
