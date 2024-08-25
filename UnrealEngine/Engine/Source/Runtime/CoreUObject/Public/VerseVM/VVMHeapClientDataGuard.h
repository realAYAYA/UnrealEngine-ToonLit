// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMHeapPageHeader.h"

namespace Verse
{

struct FHeapClientDataGuard final
{
	FHeapClientDataGuard(FHeapPageHeader* InHeader)
		: Header(InHeader)
		, ClientDataPtr(Header->LockClientData())
	{
	}

	~FHeapClientDataGuard()
	{
		Header->UnlockClientData();
	}

	FHeapPageHeader* GetHeader() const
	{
		return Header;
	}

	void** GetClientDataPtr() const
	{
		return ClientDataPtr;
	}

private:
	FHeapPageHeader* Header;
	void** ClientDataPtr;
};

} // namespace Verse
#endif // WITH_VERSE_VM
