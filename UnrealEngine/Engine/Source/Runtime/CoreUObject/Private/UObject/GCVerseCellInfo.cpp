// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/GCVerseCellInfo.h"

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
FGCVerseCellInfo* FGCVerseCellInfo::FindOrAddInfoHelper(const Verse::VCell* InCell, TMap<const Verse::VCell*, FGCVerseCellInfo*>& InOutVerseCellToInfoMap)
{
	if (FGCVerseCellInfo** ExistingCellInfo = InOutVerseCellToInfoMap.Find(InCell))
	{
		return *ExistingCellInfo;
	}

	FGCVerseCellInfo* NewInfo = new FGCVerseCellInfo(InCell);
	InOutVerseCellToInfoMap.Add(InCell, NewInfo);

	return NewInfo;
};
#endif