// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "Containers/HashTable.h"

class FUniquePageList
{
public:
			FUniquePageList();

	void	Initialize();

	void	Add( uint32 Page, uint32 Count );

	uint32	GetNum() const					{ return NumPages; }
	uint32	GetPage( uint32 Index ) const	{ return Pages[ Index ]; }
	uint32	GetCount( uint32 Index ) const	{ return Counts[ Index ]; }

	void	MergePages(const FUniquePageList* RESTRICT Other);

private:
	enum
	{
		HashSize		= 16*1024,
		MaxUniquePages	= 8*1024,
	};

	bool bInitialized;
	uint32 NumPages;
	uint32 MaxNumCollisions;
	uint16 HashIndices[HashSize];
	uint32 Pages[ MaxUniquePages ];
	uint16 Counts[ MaxUniquePages ];
};

FUniquePageList::FUniquePageList()
	: bInitialized( false )
	, NumPages( 0 )
	, MaxNumCollisions( 0 )
{}

void FUniquePageList::Initialize()
{
	if (!bInitialized)
	{
		FMemory::Memset(HashIndices, 0xff);
		bInitialized = true;
	}
}

void FUniquePageList::Add( uint32 Page, uint32 Count )
{
	uint32 HashIndex = MurmurFinalize32(Page) & (HashSize - 1u);
	uint32 NumCollisions = 0u;
	while (true)
	{
		uint32 PageIndex = HashIndices[HashIndex];
		if (PageIndex == 0xffff)
		{
			if (NumPages < MaxUniquePages)
			{
				PageIndex = NumPages++;
				HashIndices[HashIndex] = PageIndex;
				Pages[PageIndex] = Page;
				Counts[PageIndex] = Count;
			}
			break;
		}
		else if (Pages[PageIndex] == Page)
		{
			const uint32 PrevCount = Counts[PageIndex];
			Counts[PageIndex] = FMath::Min<uint32>(PrevCount + Count, 0xffff);
			break;
		}
		HashIndex = (HashIndex + 1u) & (HashSize - 1u);
		++NumCollisions;
	}
#if DO_GUARD_SLOW
	MaxNumCollisions = FMath::Max(MaxNumCollisions, NumCollisions);
#endif // DO_GUARD_SLOW
}

void FUniquePageList::MergePages(const FUniquePageList* RESTRICT Other)
{
	for (uint32 Index = 0u; Index < Other->NumPages; ++Index)
	{
		Add(Other->Pages[Index], Other->Counts[Index]);
	}
}
