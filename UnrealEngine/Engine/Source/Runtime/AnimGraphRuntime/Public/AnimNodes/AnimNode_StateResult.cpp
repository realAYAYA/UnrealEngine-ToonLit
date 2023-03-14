// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_StateResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_StateResult)

bool FAnimNode_StateResult::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if(Tag.Type == NAME_StructProperty && Tag.StructName == FAnimNode_Root::StaticStruct()->GetFName())
	{
		FAnimNode_Root OldValue;
		FAnimNode_Root::StaticStruct()->SerializeItem(Slot, &OldValue, nullptr);
		*static_cast<FAnimNode_Root*>(this) = OldValue;

		return true;
	}

	return false;
}

int32 FAnimNode_StateResult::GetStateIndex() const
{
	return GET_ANIM_NODE_DATA(int32, StateIndex);
}

