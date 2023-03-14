// Copyright Epic Games, Inc. All Rights Reserved.

#include "FunctionCaller.h"

#if WITH_EDITORONLY_DATA

#include "LevelVariantSets.h"
#include "Variant.h"
#include "VariantSet.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "UObject/PropertyPortFlags.h"

// This file is based on MovieSceneEvent.cpp

UK2Node_FunctionEntry* FFunctionCaller::GetFunctionEntry() const
{
	return CastChecked<UK2Node_FunctionEntry>(FunctionEntry.Get(), ECastCheckedType::NullAllowed);
}

void FFunctionCaller::SetFunctionEntry(UK2Node_FunctionEntry* InFunctionEntry)
{
	FunctionEntry = InFunctionEntry;
	CacheFunctionName();
}

bool FFunctionCaller::IsBoundToBlueprint() const
{
	return IsValidFunction(GetFunctionEntry());
}

bool FFunctionCaller::IsValidFunction(UK2Node_FunctionEntry* Function)
{
	if (!IsValid(Function) || !IsValid(Function->GetGraph()))
	{
		return false;
	}
	else if (Function->UserDefinedPins.Num() == 0)
	{
		return true;
	}
	else if (Function->UserDefinedPins.Num() == 1)
	{
		TSharedPtr<FUserPinInfo> TargetPin = Function->UserDefinedPins[0];
		if (TargetPin->PinType.bIsReference ||
			TargetPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object)
		{
			return false;
		}

		return true;
	}
	else if (Function->UserDefinedPins.Num() == 4)
	{
		TSharedPtr<FUserPinInfo> TargetPin			 = Function->UserDefinedPins[0];
		TSharedPtr<FUserPinInfo> LevelVariantSetsPin = Function->UserDefinedPins[1];
		TSharedPtr<FUserPinInfo> VariantSetPin		 = Function->UserDefinedPins[2];
		TSharedPtr<FUserPinInfo> VariantPin			 = Function->UserDefinedPins[3];

		if (TargetPin->PinType.bIsReference ||
			TargetPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object)
		{
			return false;
		}
		if (LevelVariantSetsPin->PinType.bIsReference ||
			LevelVariantSetsPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object ||
			LevelVariantSetsPin->PinType.PinSubCategoryObject != ULevelVariantSets::StaticClass())
		{
			return false;
		}
		if (VariantSetPin->PinType.bIsReference ||
			VariantSetPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object ||
			VariantSetPin->PinType.PinSubCategoryObject != UVariantSet::StaticClass())
		{
			return false;
		}
		if (VariantPin->PinType.bIsReference ||
			VariantPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object ||
			VariantPin->PinType.PinSubCategoryObject != UVariant::StaticClass())
		{
			return false;
		}

		return true;
	}

	return false;
}

uint32 FFunctionCaller::GetDisplayOrder() const
{
	return DisplayOrder;
}

void FFunctionCaller::SetDisplayOrder(uint32 InDisplayOrder)
{
	DisplayOrder = InDisplayOrder;
}

void FFunctionCaller::CacheFunctionName()
{
	UK2Node_FunctionEntry* Node = GetFunctionEntry();

	FunctionName = NAME_None;

	if (IsValidFunction(Node))
	{
		UEdGraph* Graph = Node->GetGraph();
		if (Graph)
		{
			FunctionName = Graph->GetFName();
		}
	}
}

#endif // WITH_EDITORONLY_DATA


void FFunctionCaller::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		CacheFunctionName();
	}
#endif
}


