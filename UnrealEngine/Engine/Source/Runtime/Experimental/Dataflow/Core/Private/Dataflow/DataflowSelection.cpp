// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSelection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowSelection)

void FDataflowSelection::Initialize(int32 NumBits, bool Value) 
{ 
	SelectionArray.AddUninitialized(NumBits); 
	SelectionArray.SetRange(0, NumBits, Value); 
}

void FDataflowSelection::Initialize(const FDataflowSelection& Other)
{
	SelectionArray = Other.SelectionArray;
}

void FDataflowSelection::AsArray(TArray<int32>& SelectionArr) const
{
	SelectionArr.Reset(SelectionArray.Num());

	TBitArray<>::FConstIterator It(SelectionArray);
	while (It)
	{
		int32 Index = It.GetIndex();

		if (SelectionArray[Index])
		{
			SelectionArr.Add(Index);
		}

		++It;
	}
}

TArray<int32> FDataflowSelection::AsArray() const
{
	TArray<int32> SelectionArr;

	TBitArray<>::FConstIterator It(SelectionArray);
	while (It)
	{
		int32 Index = It.GetIndex();

		if (SelectionArray[Index])
		{
			SelectionArr.Add(Index);
		}

		++It;
	}

	return SelectionArr;
}

void FDataflowSelection::SetFromArray(const TArray<int32>& SelectionArr)
{
	SelectionArray.Init(false, SelectionArray.Num());

	for (int32 Elem : SelectionArr)
	{
		SelectionArray[Elem] = true;
	}
}

void FDataflowSelection::SetFromArray(const TArray<bool>& SelectionArr)
{
	SelectionArray.Init(false, SelectionArray.Num());

	for (int32 Idx = 0; Idx < SelectionArr.Num(); ++Idx)
	{
		if (SelectionArr[Idx])
		{
			SetSelected(Idx);
		}
	}
}

void FDataflowSelection::AND(const FDataflowSelection& Other, FDataflowSelection& Result) const
{ 
	Result.SelectionArray = TBitArray<>::BitwiseAND(SelectionArray, Other.SelectionArray, EBitwiseOperatorFlags::MaxSize);
}

void FDataflowSelection::OR(const FDataflowSelection& Other, FDataflowSelection& Result) const
{
	Result.SelectionArray = TBitArray<>::BitwiseOR(SelectionArray, Other.SelectionArray, EBitwiseOperatorFlags::MaxSize);
}

void FDataflowSelection::XOR(const FDataflowSelection& Other, FDataflowSelection& Result) const
{
	Result.SelectionArray = TBitArray<>::BitwiseXOR(SelectionArray, Other.SelectionArray, EBitwiseOperatorFlags::MaxSize);
}

void FDataflowSelection::Subtract(const FDataflowSelection& Other, FDataflowSelection& Result) const
{
	TBitArray<> InvSelectionArray = Other.SelectionArray;
	InvSelectionArray.BitwiseNOT();
	Result.SelectionArray = TBitArray<>::BitwiseAND(SelectionArray, InvSelectionArray, EBitwiseOperatorFlags::MaxSize);
}

int32 FDataflowSelection::NumSelected() const
{
	return SelectionArray.CountSetBits(0, SelectionArray.Num());
}

bool FDataflowSelection::AnySelected() const
{
	TBitArray<>::FConstIterator It(SelectionArray);
	while (It)
	{
		if (It.GetValue())
		{
			return true;
		}

		++It;
	}

	return false;
}

void FDataflowSelection::SetWithMask(const bool Value, const FDataflowSelection& Mask)
{
	if (SelectionArray.Num() == Mask.Num())
	{
		for (int32 Idx = 0; Idx < SelectionArray.Num(); ++Idx)
		{
			if (Mask.IsSelected(Idx))
			{
				SelectionArray[Idx] = Value;
			}
		}
	}
}

