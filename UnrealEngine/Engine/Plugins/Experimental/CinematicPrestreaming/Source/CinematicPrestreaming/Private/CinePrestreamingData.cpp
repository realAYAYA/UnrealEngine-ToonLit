// Copyright Epic Games, Inc. All Rights Reserved.

#include "CinePrestreamingData.h"

void UCinePrestreamingData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	int32 Size = sizeof(UCinePrestreamingData);
	Size += Times.GetAllocatedSize();
	Size += VirtualTextureDatas.GetAllocatedSize();
	for (FCinePrestreamingVTData const& Data : VirtualTextureDatas)
	{
		Size += Data.PageIds.GetAllocatedSize();
	}

	Size += NaniteDatas.GetAllocatedSize();
	for (FCinePrestreamingNaniteData const& Data : NaniteDatas)
	{
		Size += Data.RequestData.GetAllocatedSize();
	}
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Size);
}
