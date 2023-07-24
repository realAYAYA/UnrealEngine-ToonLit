﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefinitionProvider.h"
#include "HAL/UnrealMemory.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{

////////////////////////////////////////////////////////////////////////////////////////////////////

const FName FDefinitionProvider::ProviderName(TEXT("DefinitionProvider"));

////////////////////////////////////////////////////////////////////////////////////////////////////

FDefinitionProvider::FDefinitionProvider(IAnalysisSession* InSession)
	: PageRemain(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FDefinitionProvider::AddEntry(uint64 Hash, const void* Ptr)
{
	Definitions.Add(Hash, Ptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const void* FDefinitionProvider::FindEntry(uint64 Hash) const
{
	return Definitions.FindRef(Hash);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void* FDefinitionProvider::Allocate(uint32 Size, uint32 Alignment)
{
	check(Size);
	check(Size <= PageSize);
	uint8* Dest = nullptr;
	if (PageRemain > Size)
	{
		Dest = Pages.Last().Get() + (PageSize - PageRemain);
	}
	else
	{
		auto& NewPage = Pages.Emplace_GetRef((uint8*)FMemory::MallocZeroed(PageSize));

		Dest = NewPage.Get();
		PageRemain = PageSize;
	}
	PageRemain -= Size;
	return Dest;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

IDefinitionProvider* GetDefinitionProvider(IAnalysisSession& Session)
{
	return Session.EditProvider<IDefinitionProvider>(FDefinitionProvider::ProviderName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const IDefinitionProvider* ReadDefinitionProvider(const IAnalysisSession& Session)
{
	return Session.ReadProvider<IDefinitionProvider>(FDefinitionProvider::ProviderName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace TraceServices
