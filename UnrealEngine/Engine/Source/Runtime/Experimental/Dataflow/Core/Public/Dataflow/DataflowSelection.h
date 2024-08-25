// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataflowSelection.generated.h"

USTRUCT()
struct FDataflowSelection
{
	GENERATED_USTRUCT_BODY()

	DATAFLOWCORE_API void Initialize(int32 NumBits, bool Value);
	DATAFLOWCORE_API void Initialize(const FDataflowSelection& Other);
	int32 Num() const { return SelectionArray.Num(); }
	DATAFLOWCORE_API int32 NumSelected() const;
	DATAFLOWCORE_API bool AnySelected() const;
	bool IsSelected(int32 Idx) const { return SelectionArray[Idx]; }
	void SetSelected(int32 Idx) { SelectionArray[Idx] = true; }
	void SetNotSelected(int32 Idx) { SelectionArray[Idx] = false; }
	DATAFLOWCORE_API void AsArray(TArray<int32>& SelectionArr) const;
	DATAFLOWCORE_API TArray<int32> AsArray() const;
	
	/**
	* Sets the selection from a sparse array (it only contains the indices of the selected items)
	*/
	DATAFLOWCORE_API void SetFromArray(const TArray<int32>& SelectionArr);
	
	/**
	* Sets the selection from a dense array (it contains a true/false element for every item)
	* (for example from the "Internal" attr from FacesGroup)
	*/
	DATAFLOWCORE_API void SetFromArray(const TArray<bool>& SelectionArr);
	DATAFLOWCORE_API void AND(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	DATAFLOWCORE_API void OR(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	DATAFLOWCORE_API void XOR(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	// subtract the selected elements of 'Other' from this selection
	DATAFLOWCORE_API void Subtract(const FDataflowSelection& Other, FDataflowSelection& Result) const;
	void Invert() { SelectionArray.BitwiseNOT(); }
	DATAFLOWCORE_API void SetWithMask(const bool Value, const FDataflowSelection& Mask);
	const TBitArray<>& GetBitArray() const { return SelectionArray; };

private:

	TBitArray<> SelectionArray;
};

USTRUCT()
struct FDataflowTransformSelection : public FDataflowSelection
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct FDataflowVertexSelection : public FDataflowSelection
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct FDataflowFaceSelection : public FDataflowSelection
{
	GENERATED_USTRUCT_BODY()
};

