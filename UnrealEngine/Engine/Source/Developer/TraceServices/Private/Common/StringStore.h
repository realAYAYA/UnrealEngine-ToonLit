// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Common/SlabAllocator.h"
#include "Common/PagedArray.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "HAL/CriticalSection.h"

namespace TraceServices
{

class FStringStore
{
public:
	FStringStore(FSlabAllocator& Allocator);
	const TCHAR* Store(const TCHAR* String);
	const TCHAR* Store(const FStringView& String);

private:
	enum
	{
		BlockSize = 4 << 20
	};
	FCriticalSection Cs;
	FSlabAllocator& Allocator;
	TMap<uint32, const TCHAR*> StoredStrings;
	TCHAR* BufferPtr = nullptr;
	uint64 BufferLeft = 0;
	uint64 BlockCount = 0;

};

} // namespace TraceServices
