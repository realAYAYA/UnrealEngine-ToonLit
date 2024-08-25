// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeIndexTypes.h"

bool FStateTreeIndex16::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Support loading from Index8.
	if (Tag.GetType().IsStruct(FStateTreeIndex8::StaticStruct()->GetFName()))
	{
		FStateTreeIndex8 OldValue;
		FStateTreeIndex8::StaticStruct()->SerializeItem(Slot, &OldValue, nullptr);

		int32 NewValue = OldValue.AsInt32();
		if (!IsValidIndex(NewValue))
		{
			NewValue = INDEX_NONE;
		}
		
		*this = FStateTreeIndex16(NewValue);
		
		return true;
	}
	
	return false;
}


bool FStateTreeIndex8::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Support loading from Index16.
	if (Tag.GetType().IsStruct(FStateTreeIndex16::StaticStruct()->GetFName()))
	{
		FStateTreeIndex16 OldValue;
		FStateTreeIndex16::StaticStruct()->SerializeItem(Slot, &OldValue, nullptr);

		int32 NewValue = OldValue.AsInt32();
		if (!IsValidIndex(NewValue))
		{
			NewValue = INDEX_NONE;
		}
		
		*this = FStateTreeIndex8(NewValue);
		
		return true;
	}
	
	return false;
}