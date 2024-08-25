// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Items/OperatorStackEditorItem.h"
#include "Templates/SharedPointer.h"

/** Represent the current context we customize for the whole operator stack */
struct FOperatorStackEditorContext final
{
	FOperatorStackEditorContext() = default;
	explicit FOperatorStackEditorContext(const TArray<TSharedPtr<FOperatorStackEditorItem>>& InItems)
		: Items(InItems)
	{
	}

	/** Items we want to customize */
	TConstArrayView<TSharedPtr<FOperatorStackEditorItem>> GetItems() const
	{
		return Items;
	}

protected:
	/** Context items being customized */
	TArray<TSharedPtr<FOperatorStackEditorItem>> Items;
};

typedef TSharedPtr<FOperatorStackEditorContext> FOperatorStackEditorContextPtr;
typedef TWeakPtr<FOperatorStackEditorContext> FOperatorStackEditorContextPtrWeak;