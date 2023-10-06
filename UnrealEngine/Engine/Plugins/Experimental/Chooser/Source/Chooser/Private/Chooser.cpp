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
void UChooserTable::PostEditUndo()
{
	UObject::PostEditUndo();

	if (CachedPreviousOutputObjectType != OutputObjectType || CachedPreviousResultType != ResultType)
	{
		OnOutputObjectTypeChanged.Broadcast(OutputObjectType);
		CachedPreviousOutputObjectType = OutputObjectType;
		CachedPreviousResultType = ResultType;
	}
	OnContextClassChanged.Broadcast();
}

void UChooserTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	
	static FName OutputObjectTypeName = "OutputObjectType";
	static FName ResultTypeName = "ResultType";
	if (PropertyChangedEvent.Property->GetName() == OutputObjectTypeName)
	{
		if (CachedPreviousOutputObjectType != OutputObjectType)
		{
			OnOutputObjectTypeChanged.Broadcast(OutputObjectType);
			CachedPreviousOutputObjectType = OutputObjectType;
		}
	}
	else if (PropertyChangedEvent.Property->GetName() == ResultTypeName)
	{
		if (CachedPreviousResultType != ResultType)
		{
			OnOutputObjectTypeChanged.Broadcast(OutputObjectType);
			CachedPreviousResultType = ResultType;
		}
	}
	else
	{
		OnContextClassChanged.Broadcast();
	}
}

void UChooserTable::PostLoad()
{
	Super::PostLoad();

	CachedPreviousOutputObjectType = OutputObjectType;
	CachedPreviousResultType = ResultType;

	// convert old data if it exists

	if (ContextObjectType_DEPRECATED)
	{
		ContextData.SetNum(1);
		ContextData[0].InitializeAs<FContextObjectTypeClass>();
		FContextObjectTypeClass& Context = ContextData[0].GetMutable<FContextObjectTypeClass>();
		Context.Class = ContextObjectType_DEPRECATED;
		Context.Direction = EContextObjectDirection::ReadWrite;
		ContextObjectType_DEPRECATED = nullptr;
		OnContextClassChanged.Broadcast();
	}

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

void UChooserTable::IterateRecentContextObjects(TFunction<void(const UObject*)> Callback) const
{
	FScopeLock Lock(&DebugLock);
	for(TWeakObjectPtr<const UObject>& Object : RecentContextObjects)
	{
		if (Object.IsValid())
		{
			Callback(Object.Get());
		}
	}
}

void UChooserTable::UpdateDebugging(FChooserEvaluationContext& Context) const
{
	FScopeLock Lock(&DebugLock);
	for (const FInstancedStruct& Param : Context.Params)
	{
		if (const FChooserEvaluationInputObject* ObjectParam = Param.GetPtr<FChooserEvaluationInputObject>())
		{
			UObject* ContextObject = ObjectParam->Object;
			RecentContextObjects.Add(MakeWeakObjectPtr(ContextObject));
			if (ContextObject == DebugTarget)
			{
				bDebugTestValuesValid = true;
				Context.DebuggingInfo.bCurrentDebugTarget = true;
				return;
			}
		}
	}
	Context.DebuggingInfo.bCurrentDebugTarget = false;
}

#endif

FObjectChooserBase::EIteratorStatus UChooserTable::EvaluateChooser(FChooserEvaluationContext& Context, const UChooserTable* Chooser, FObjectChooserBase::FObjectChooserIteratorCallback Callback)
{
	if (Chooser == nullptr)
	{
		return FObjectChooserBase::EIteratorStatus::Continue;
	}

	// todo validate that parameter types in context data match

#if WITH_EDITOR
	Chooser->UpdateDebugging(Context);
#endif

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
		if (Column.HasFilters())
		{
			Swap(IndicesIn, IndicesOut);
			IndicesOut->SetNum(0, false);
			Column.Filter(Context, *IndicesIn, *IndicesOut);
		}
	}
	
	// of the rows that passed all column filters, iterate through them calling the callback until it returns Stop
	bool bSetOutputs = false;
	for (uint32 SelectedIndex : *IndicesOut)
	{
		if (Chooser->ResultsStructs.Num() > (int32)SelectedIndex)
		{
			const FObjectChooserBase& SelectedResult = Chooser->ResultsStructs[SelectedIndex].Get<FObjectChooserBase>();
			FObjectChooserBase::EIteratorStatus Status = SelectedResult.ChooseMulti(Context, Callback);
			if (Status != FObjectChooserBase::EIteratorStatus::Continue)
			{
				bSetOutputs = true;
				// trigger all output columns
				for (const FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
				{
					const FChooserColumnBase& Column = ColumnData.Get<FChooserColumnBase>();
					Column.SetOutputs(Context, SelectedIndex);
				}
#if WITH_EDITOR
				if (Context.DebuggingInfo.bCurrentDebugTarget)
				{
					Chooser->SetDebugSelectedRow(SelectedIndex);
				}
#endif
			}
			if (Status == FObjectChooserBase::EIteratorStatus::Stop)
			{
				return FObjectChooserBase::EIteratorStatus::Stop;
			}
		}
	}

	// If this is a nested chooser make sure the parent also sets the output vales from the row that contained this chooser
	return bSetOutputs ? FObjectChooserBase::EIteratorStatus::ContinueWithOutputs : FObjectChooserBase::EIteratorStatus::Continue;
}

UObject* FEvaluateChooser::ChooseObject(FChooserEvaluationContext& Context) const
{
	UObject* Result = nullptr;
	UChooserTable::EvaluateChooser(Context, Chooser, FObjectChooserIteratorCallback::CreateLambda([&Result](UObject* InResult)
	{
		Result = InResult;
		return FObjectChooserBase::EIteratorStatus::Stop;
	}));

	return Result;
}

FObjectChooserBase::EIteratorStatus FEvaluateChooser::ChooseMulti(FChooserEvaluationContext& Context, FObjectChooserIteratorCallback Callback) const
{
	return UChooserTable::EvaluateChooser(Context, Chooser, Callback);
}