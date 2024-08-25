// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Containers/Map.h"

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCell.h"

namespace Verse
{
	struct VCell;
}

/**
 * Structure containing information about a VCell participating in Garbage Collection.
 * It's purpose is to avoid holding onto direct references to cells which may have already been Garbage Collected.
 * FGCVerseCellInfo interface mimics that of VCell.
 **/
class FGCVerseCellInfo
{
public:

	FGCVerseCellInfo() = default;
	explicit FGCVerseCellInfo(const Verse::VCell* Cell)
		: DebugName(Cell->DebugName())
	{
	}

private:

	/** Name of the object */
	FName DebugName;

public:

	FName GetDebugName() const
	{
		return DebugName;
	}

	/** Helper function for adding info about an VCell into VCell to FGCVerseCellInfo map */
	static COREUOBJECT_API FGCVerseCellInfo* FindOrAddInfoHelper(const Verse::VCell* InCell, TMap<const Verse::VCell*, FGCVerseCellInfo*>& InOutVerseCellToInfoMap);
};
#endif