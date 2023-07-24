// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Core/Group.h"

#include "Containers/List.h"


#define DEBUG_FACTORY

namespace UE::CADKernel
{
const int32 MaxSize = 256;

template<class ElementType>
class TFactory
{
protected:
	TDoubleLinkedList<TArray<ElementType>> StackOfEntitySet;
	TArray<ElementType*> FreeEntityStack;
#ifdef DEBUG_FACTORY
	int32 AllocatedElementNum = 0;
	int32 UsedElementNum = 0;
	int32 AvailableElementNum = 0;
#endif

private:
	void FillFreeEntityStack()
	{
		StackOfEntitySet.AddHead(TArray<ElementType>());
		TArray<ElementType>& EntityPool = StackOfEntitySet.GetHead()->GetValue();
		EntityPool.SetNum(MaxSize);
		for (ElementType& Entity : EntityPool)
		{
			FreeEntityStack.Add(&Entity);
		}
		AllocatedElementNum += MaxSize;
	}

public:
	TFactory()
	{
		FreeEntityStack.Reserve(MaxSize);
	}

	void DeleteEntity(ElementType* Entity)
	{
		Entity->Clean();
		FreeEntityStack.Add(Entity);
		UsedElementNum--;
		ensureCADKernel(UsedElementNum + FreeEntityStack.Num() == AllocatedElementNum);
	}

	ElementType& New()
	{
		ensureCADKernel(UsedElementNum + FreeEntityStack.Num() == AllocatedElementNum);
		if (FreeEntityStack.IsEmpty())
		{
			FillFreeEntityStack();
		}
		UsedElementNum++;
		return *FreeEntityStack.Pop(false);
	}
};
} // namespace UE::CADKernel

