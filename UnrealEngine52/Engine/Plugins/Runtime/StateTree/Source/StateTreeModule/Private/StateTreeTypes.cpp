// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTypes.h"
#include "StateTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTypes)

DEFINE_LOG_CATEGORY(LogStateTree);

const FStateTreeStateHandle FStateTreeStateHandle::Invalid = FStateTreeStateHandle();
const FStateTreeStateHandle FStateTreeStateHandle::Succeeded = FStateTreeStateHandle(FStateTreeStateHandle::SucceededIndex);
const FStateTreeStateHandle FStateTreeStateHandle::Failed = FStateTreeStateHandle(FStateTreeStateHandle::FailedIndex);
const FStateTreeStateHandle FStateTreeStateHandle::Root = FStateTreeStateHandle(0);

const FStateTreeIndex16 FStateTreeIndex16::Invalid = FStateTreeIndex16();
const FStateTreeIndex8 FStateTreeIndex8::Invalid = FStateTreeIndex8();

const FStateTreeExternalDataHandle FStateTreeExternalDataHandle::Invalid = FStateTreeExternalDataHandle();


bool FStateTreeIndex16::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Support loading from Index8.
	if (Tag.Type == NAME_StructProperty
		&& Tag.StructName == FStateTreeIndex8::StaticStruct()->GetFName())
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
	if (Tag.Type == NAME_StructProperty
		&& Tag.StructName == FStateTreeIndex16::StaticStruct()->GetFName())
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

//////////////////////////////////////////////////////////////////////////
// FStateTreeStateLink

bool FStateTreeStateLink::Serialize(FStructuredArchive::FSlot Slot)
{
	Slot.GetUnderlyingArchive().UsingCustomVersion(FStateTreeCustomVersion::GUID);
	return false; // Let the default serializer handle serializing.
}

void FStateTreeStateLink::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	const int32 CurrentVersion = Ar.CustomVer(FStateTreeCustomVersion::GUID);
	if (CurrentVersion < FStateTreeCustomVersion::AddedExternalTransitions)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		LinkType = Type_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
#endif // WITH_EDITORONLY_DAT
}
