// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "UObject/Class.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMShape.h"

// Class used for all VerseVM generated classes
class UVerseVMClass : public UClass
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UVerseVMClass, UClass, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UVerseVMClass, COREUOBJECT_API)
	DECLARE_WITHIN_UPACKAGE()

public:
	COREUOBJECT_API UVerseVMClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FProperty* GetPropertyForField(Verse::FAllocationContext Context, Verse::VUniqueString& FieldName) const;

	Verse::TWriteBarrier<Verse::VShape> Shape;
};
