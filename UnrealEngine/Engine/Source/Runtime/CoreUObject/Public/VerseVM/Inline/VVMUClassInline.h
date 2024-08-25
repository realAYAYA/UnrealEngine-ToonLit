// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMUClass.h"

FORCEINLINE_DEBUGGABLE FProperty* UVerseVMClass::GetPropertyForField(Verse::FAllocationContext Context, Verse::VUniqueString& FieldName) const
{
	using namespace Verse;

	const VShape::VEntry* Field = Shape->GetField(Context, FieldName);
	if (!Field)
	{
		V_DIE("Field: %hs was not found!", FieldName.AsCString());
	}
	checkSlow(Field->Type == EFieldType::FProperty);
	return Field->Property;
}

#endif // WITH_VERSE_VM
