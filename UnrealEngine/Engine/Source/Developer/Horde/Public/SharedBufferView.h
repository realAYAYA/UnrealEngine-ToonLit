// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Memory/SharedBuffer.h"

//
// View into a buffer with shared ownership
//
class HORDE_API FSharedBufferView final
{
public:
	FSharedBufferView();
	FSharedBufferView(FSharedBuffer InBuffer);
	FSharedBufferView(FSharedBuffer InBuffer, const FMemoryView& InView);
	FSharedBufferView(FSharedBuffer InBuffer, size_t InOffset, size_t InLength);
	~FSharedBufferView();

	static FSharedBufferView Copy(const FMemoryView& View);

	FSharedBufferView Slice(uint64 Offset) const;
	FSharedBufferView Slice(uint64 Offset, uint64 Length) const;

	const unsigned char* GetPointer() const;
	size_t GetLength() const;
	FMemoryView GetView() const;

private:
	FSharedBuffer Buffer;
	FMemoryView View;
};
