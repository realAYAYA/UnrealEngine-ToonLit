// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OperatorStackEditorItem.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"

/** Primitive version of item */
struct FOperatorStackEditorPrimitiveItem : FOperatorStackEditorItem
{
	template<
		typename InItemDefinition
		UE_REQUIRES(TIsPODType<InItemDefinition>::Value)>
	explicit FOperatorStackEditorPrimitiveItem(FFieldClass* InFieldClass, InItemDefinition& InValue)
		: FOperatorStackEditorItem(
			FOperatorStackEditorItemType(InFieldClass, EOperatorStackEditorItemType::Primitive)
		)
	{
		MemoryPtr = reinterpret_cast<uint8*>(&InValue);
		CachedHash = GetTypeHash(InValue);
	}

	virtual bool HasValue() const override
	{
		return MemoryPtr != nullptr;
	}

protected:
	virtual uint32 GetHash() const override
	{
		return CachedHash;
	}

	virtual void* GetValuePtr() const override
	{
		return MemoryPtr;
	}

	uint32 CachedHash = 0;
	uint8* MemoryPtr = nullptr;
};