// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "ChooserPropertyAccess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Chooser)

UChooserTable::UChooserTable(const FObjectInitializer& Initializer)
	:Super(Initializer)
{

}

#if WITH_EDITOR
void UChooserTable::PostLoad()
{
	Super::PostLoad();

	// convert old data if it exists

	if (Results_DEPRECATED.Num() > 0 || Columns_DEPRECATED.Num() > 0)
	{
		ResultsStructs.Reserve(Results_DEPRECATED.Num());
		ColumnsStructs.Reserve(Columns_DEPRECATED.Num());
		
		for(TScriptInterface<IObjectChooser>& Result : Results_DEPRECATED)
		{
			ResultsStructs.SetNum(ResultsStructs.Num() + 1);
			IObjectChooser* ResultInterface = Result.GetInterface();
			if (ResultInterface)
			{
				ResultInterface->ConvertToInstancedStruct(ResultsStructs.Last());
			}
		}
		
		for(TScriptInterface<IChooserColumn>& Column : Columns_DEPRECATED)
		{
			ColumnsStructs.SetNum(ColumnsStructs.Num() + 1);
			IChooserColumn* ColumnInterface = Column.GetInterface();
			if (ColumnInterface)
			{
				ColumnInterface->ConvertToInstancedStruct(ColumnsStructs.Last());
			}
		}

		Results_DEPRECATED.SetNum(0);
		Columns_DEPRECATED.SetNum(0);
	}

	// call PostLoad on Columns
	for (FInstancedStruct& ColumnData : ColumnsStructs)
	{
		if (ColumnData.IsValid())
		{
			FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
			Column.PostLoad();
		}
	}
		
}
#endif

static FObjectChooserBase::EIteratorStatus StaticEvaluateChooser(const UObject* ContextObject, const UChooserTable* Chooser, FObjectChooserBase::FObjectChooserIteratorCallback Callback)
{
	if (Chooser == nullptr)
	{
		return FObjectChooserBase::EIteratorStatus::Continue;
	}

	TArray<uint32> Indices1;
	TArray<uint32> Indices2;

	int RowCount = Chooser->ResultsStructs.Num();
	Indices1.SetNum(RowCount);
	for(int i=0;i<RowCount;i++)
	{
		Indices1[i]=i;
	}
	TArray<uint32>* IndicesOut = &Indices1;
	TArray<uint32>* IndicesIn = &Indices2;

	for (const FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
	{
		const FChooserColumnBase& Column = ColumnData.Get<FChooserColumnBase>(); 
		Swap(IndicesIn, IndicesOut);
		IndicesOut->SetNum(0, false);
		Column.Filter(ContextObject, *IndicesIn, *IndicesOut);
	}

	// of the rows that passed all column filters, return the first one for which the result row succeeds (could fail eg for a nexted chooser where no rows passed)
	for (uint32 SelectedIndex : *IndicesOut)
	{
		if (Chooser->ResultsStructs.Num() > (int32)SelectedIndex)
		{
			const FObjectChooserBase& SelectedResult = Chooser->ResultsStructs[SelectedIndex].Get<FObjectChooserBase>();
			if (SelectedResult.ChooseMulti(ContextObject, Callback) == FObjectChooserBase::EIteratorStatus::Stop)
			{
				return FObjectChooserBase::EIteratorStatus::Stop;
			}
		}
	}
	
	return FObjectChooserBase::EIteratorStatus::Continue;
}

UObject* FEvaluateChooser::ChooseObject(const UObject* ContextObject) const
{
	UObject* Result = nullptr;
	StaticEvaluateChooser(ContextObject, Chooser, FObjectChooserIteratorCallback::CreateLambda([&Result](UObject* InResult)
	{
		Result = InResult;
		return FObjectChooserBase::EIteratorStatus::Stop;
	}));

	return Result;
}

FObjectChooserBase::EIteratorStatus FEvaluateChooser::ChooseMulti(const UObject* ContextObject, FObjectChooserIteratorCallback Callback) const
{
	return StaticEvaluateChooser(ContextObject, Chooser, Callback);
}

UChooserFunctionLibrary::UChooserFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UObject* UChooserFunctionLibrary::EvaluateChooser(const UObject* ContextObject, const UChooserTable* Chooser)
{
	UObject* Result = nullptr;
	StaticEvaluateChooser(ContextObject, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result](UObject* InResult)
	{
		Result = InResult;
		return FObjectChooserBase::EIteratorStatus::Stop;
	}));

	return Result;
}

TArray<UObject*> UChooserFunctionLibrary::EvaluateChooserMulti(const UObject* ContextObject, const UChooserTable* Chooser)
{
	TArray<UObject*> Result;
	StaticEvaluateChooser(ContextObject, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result](UObject* InResult)
	{
		Result.Add(InResult);
		return FObjectChooserBase::EIteratorStatus::Continue;
	}));

	return Result;
}