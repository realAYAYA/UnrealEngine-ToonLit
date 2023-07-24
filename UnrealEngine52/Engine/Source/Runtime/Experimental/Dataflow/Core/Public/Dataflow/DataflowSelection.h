// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataflowSelection.generated.h"

USTRUCT()
struct DATAFLOWCORE_API FDataflowSelection
{
	GENERATED_USTRUCT_BODY()

	void Initialize(int32 NumBits, bool Value);
	void Initialize(const FDataflowSelection& Other);
	int32 Num() const { return SelectionArray.Num(); }
	int32 NumSelected() const;
	bool AnySelected() const;
	bool IsSelected(int32 Idx) const { return SelectionArray[Idx]; }
	void SetSelected(int32 Idx) { SelectionArray[Idx] = true; }
	void SetNotSelected(int32 Idx) { SelectionArray[Idx] = false; }
	void AsArray(TArray<int32>& SelectionArr) const;
	TArray<int32> AsArray() const;
	
	/**
	* Sets the selection from a sparse array (it only contains the indices of the selected items)
	*/
	void SetFromArray(const TArray<int32>& SelectionArr);
	
	/**
	* Sets the selection from a dense array (it contains a true/false element for every item)
	* (for example from the "Internal" attr from FacesGroup)
	*/
	void SetFromArray(const TArray<bool>& SelectionArr);
	void AND(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	void OR(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	void XOR(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	void Invert() { SelectionArray.BitwiseNOT(); }
	void SetWithMask(const bool Value, const FDataflowSelection& Mask);
	const TBitArray<>& GetBitArray() const { return SelectionArray; };

private:

	TBitArray<> SelectionArray;
};

USTRUCT()
struct DATAFLOWCORE_API FDataflowTransformSelection : public FDataflowSelection
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct DATAFLOWCORE_API FDataflowVertexSelection : public FDataflowSelection
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct DATAFLOWCORE_API FDataflowFaceSelection : public FDataflowSelection
{
	GENERATED_USTRUCT_BODY()
};

