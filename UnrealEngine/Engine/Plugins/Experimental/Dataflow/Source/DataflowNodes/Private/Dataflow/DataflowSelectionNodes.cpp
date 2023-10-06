// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSelectionNodes.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowSelectionNodes)

namespace Dataflow
{
	void RegisterSelectionNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSelectionSetDataflowNode);
	}
};

void FSelectionSetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&IndicesOut))
	{
		TArray<int32> IntArray;
		TArray<FString> StrArray;
		Indices.ParseIntoArray(StrArray, *FString(" "));
		for (FString Elem : StrArray)
		{
			if (FCString::IsNumeric(*Elem))
			{
				IntArray.Add(FCString::Atoi(*Elem));
			}
		}

		Out->SetValue<DataType>(MoveTemp(IntArray), Context);
	}
}



