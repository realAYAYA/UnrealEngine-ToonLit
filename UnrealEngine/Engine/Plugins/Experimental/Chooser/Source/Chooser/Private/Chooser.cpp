// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chooser.h"
#include "DataInterfaceTypes.h"
#include "DataInterface.h"

UChooserTable::UChooserTable(const FObjectInitializer& Initializer)
	:Super(Initializer)
{

}

void UChooserColumnBool::Filter(const UE::DataInterface::FContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut)
{
	bool Result = false;
	if(UE::DataInterface::GetDataSafe(Value, Context, Result))
	{
		for(uint32 Index : IndexListIn)
		{
			if (RowValues.Num() > (int)Index)
			{
				if (Result == RowValues[Index])
				{
					IndexListOut.Push(Index);
				}
			}
		}
	}
}

void UChooserColumnFloatRange::Filter(const UE::DataInterface::FContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut)
{
	float Result = 0.0f;
	
	if(UE::DataInterface::GetDataSafe(Value, Context, Result))
	{
		for(uint32 Index : IndexListIn)
		{
			if (RowValues.Num() > (int)Index)
			{
				const FChooserFloatRangeRowData& RowValue = RowValues[Index];
				if (Result >= RowValue.Min && Result <= RowValue.Max)
				{
					IndexListOut.Push(Index);
				}
			}
		}
	}
}

bool UDataInterface_EvaluateChooser::GetDataImpl(const UE::DataInterface::FContext& DataContext) const
{
	if (Chooser == nullptr)
	{
		return false;
	}

	TArray<uint32> Indices1;
	TArray<uint32> Indices2;

	int RowCount = Chooser->Results.Num();
	Indices1.SetNum(RowCount);
	for(int i=0;i<RowCount;i++)
	{
		Indices1[i]=i;
	}
	TArray<uint32>* IndicesOut = &Indices1;
	TArray<uint32>* IndicesIn = &Indices2;

	for (auto Column: Chooser->Columns)
	{
		if (Column)
		{
			Swap(IndicesIn, IndicesOut);
			IndicesOut->SetNum(0, false);
			Column->Filter(DataContext, *IndicesIn, *IndicesOut);
		}
	}

	// of the rows that passed all column filters, return the first one for which the result row succeeds (could fail eg for a nexted chooser where no rows passed)
	for (uint32 SelectedIndex : *IndicesOut)
	{
		if (Chooser->Results.Num() > (int32)SelectedIndex)
		{
			const TScriptInterface<IDataInterface>& SelectedResult = Chooser->Results[SelectedIndex];

			// todo: GetResultRaw returns const, GetData requires non-const
			if(UE::DataInterface::GetDataSafe(SelectedResult, DataContext, const_cast<UE::DataInterface::FParam&>(DataContext.GetResultRaw())))
			{
				return true;
			}
		}
	}

	return false;
}
