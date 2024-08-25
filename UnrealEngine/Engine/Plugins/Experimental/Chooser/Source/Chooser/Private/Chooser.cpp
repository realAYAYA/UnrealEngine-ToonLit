// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "ChooserPropertyAccess.h"
#include "ChooserTrace.h"
#include "ObjectTrace.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/Blueprint.h"
#include "ChooserIndexArray.h"
#include "IChooserParameterGameplayTag.h"
#include "UObject/AssetRegistryTagsContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Chooser)

DEFINE_LOG_CATEGORY(LogChooser)


#if WITH_EDITOR
const FName UChooserTable::PropertyNamesTag = "ChooserPropertyNames";
const FString UChooserTable::PropertyTagDelimiter = TEXT(";");
#endif

UChooserTable::UChooserTable(const FObjectInitializer& Initializer)
	:Super(Initializer)
{

}

void UChooserTable::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	CachedPreviousOutputObjectType = OutputObjectType;
	CachedPreviousResultType = ResultType;
#endif

#if WITH_EDITORONLY_DATA
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
#endif

	Compile();
}

void UChooserTable::BeginDestroy()
{
	ColumnsStructs.Empty();
	ResultsStructs.Empty();
	Super::BeginDestroy();
}

#if WITH_EDITOR

void UChooserTable::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UChooserTable::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// Output property names we use
	TStringBuilder<256> PropertyNamesBuilder;
	PropertyNamesBuilder.Append(PropertyTagDelimiter);

	for(const FInstancedStruct& Column : ColumnsStructs)
	{
		if (const FChooserColumnBase* ColumnBase = Column.GetPtr<FChooserColumnBase>())
		{
			if (FChooserParameterBase* Parameter = const_cast<FChooserColumnBase*>(ColumnBase)->GetInputValue())
			{
				Parameter->AddSearchNames(PropertyNamesBuilder);
			}
		}
	}

	Context.AddTag(FAssetRegistryTag(PropertyNamesTag, PropertyNamesBuilder.ToString(), FAssetRegistryTag::TT_Hidden));
}

void UChooserTable::AddCompileDependency(const UStruct* InStructType)
{
	UStruct* StructType = const_cast<UStruct*>(InStructType);
	if (!CompileDependencies.Contains(StructType))
	{
		if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(StructType))
		{
			UserDefinedStruct->ChangedEvent.AddUObject(this, &UChooserTable::OnDependentStructChanged);
			CompileDependencies.Add(StructType);
		}
		else if (UClass* Class = Cast<UClass>(StructType))
		{
			if(UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy))
			{
				Blueprint->OnCompiled().AddUObject(this, &UChooserTable::OnDependencyCompiled);
				CompileDependencies.Add(StructType);
			}
		}
	}
}

#endif

void UChooserTable::Compile(bool bForce)
{
	IHasContextClass* ContextOwner = GetContextOwner();
	
	for (FInstancedStruct& ColumnData : ColumnsStructs)
	{
		if (ColumnData.IsValid())
		{
			FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
			Column.Compile(ContextOwner, bForce);
		}
	}

	for(FInstancedStruct& ResultData : ResultsStructs)
	{
		if (ResultData.IsValid())
		{
			FObjectChooserBase& Result = ResultData.GetMutable<FObjectChooserBase>();
			Result.Compile(ContextOwner, bForce);
		}
	}
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
	
	if (PropertyChangedEvent.Property)
	{
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
	else
	{
		OnOutputObjectTypeChanged.Broadcast(OutputObjectType);
		OnContextClassChanged.Broadcast();
	}
}

void UChooserTable::IterateRecentContextObjects(TFunction<void(const FString&)> Callback) const
{
	FScopeLock Lock(&DebugLock);
	for(const FString& ObjectName : RecentContextObjects)
	{
		Callback(ObjectName);
	}
}

void UChooserTable::UpdateDebugging(FChooserEvaluationContext& Context) const
{
	FScopeLock Lock(&DebugLock);

	const UChooserTable* ContextOwner = GetContextOwner();
	
	for (const FStructView& Param : Context.Params)
	{
		if (const FChooserEvaluationInputObject* ObjectParam = Param.GetPtr<const FChooserEvaluationInputObject>())
		{
			if (UObject* ContextObject = ObjectParam->Object)
			{
				RecentContextObjects.Add(ContextObject->GetName());
				
				if (ContextObject->GetName() == ContextOwner->GetDebugTargetName()) 
				{
					bDebugTestValuesValid = true;
					Context.DebuggingInfo.bCurrentDebugTarget = true;
					return;
				}
			}
		}
	}
	Context.DebuggingInfo.bCurrentDebugTarget = false;
}

#endif

FObjectChooserBase::EIteratorStatus UChooserTable::EvaluateChooser(FChooserEvaluationContext& Context, const UChooserTable* Chooser, FObjectChooserBase::FObjectChooserIteratorCallback Callback)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EvaluateChooser);
	
	if (Chooser == nullptr)
	{
		return FObjectChooserBase::EIteratorStatus::Continue;
	}

	// todo validate that parameter types in context data match

#if WITH_EDITOR
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EvaluateChooser_Debugging);
		Chooser->UpdateDebugging(Context);
	}
#endif
	
#if CHOOSER_DEBUGGING_ENABLED
	Context.DebuggingInfo.CurrentChooser = Chooser;
#endif

	uint32 Count = Chooser->ResultsStructs.Num();
	uint32 BufferSize = Count * sizeof(uint32);

	FChooserIndexArray Indices1(static_cast<uint32*>(FMemory_Alloca(BufferSize)), Count);
	FChooserIndexArray Indices2(static_cast<uint32*>(FMemory_Alloca(BufferSize)), Count);

	int RowCount = Chooser->ResultsStructs.Num();
	Indices1.SetNum(RowCount);
	for(int i=0;i<RowCount;i++)
	{
		Indices1[i]=i;
	}
	FChooserIndexArray* IndicesOut = &Indices1;
	FChooserIndexArray* IndicesIn = &Indices2;

	for (const FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
	{
		const FChooserColumnBase& Column = ColumnData.Get<FChooserColumnBase>();
		if (Column.HasFilters())
		{
			Swap(IndicesIn, IndicesOut);
			IndicesOut->SetNum(0);
			Column.Filter(Context, *IndicesIn, *IndicesOut);
		}
	}
	
	bool bSetOutputs = false;
	if (IndicesOut->IsEmpty())
	{
		// if no rows passed the filter columns then return the FallbackResult and output the FallbackValue from each output column
		
		#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			Chooser->SetDebugSelectedRow(-1);
		}
		#endif
		TRACE_CHOOSER_EVALUATION(Chooser, Context, -1);
	
		if (Chooser->FallbackResult.IsValid())
		{
			const FObjectChooserBase& SelectedResult = Chooser->FallbackResult.Get<FObjectChooserBase>();
			FObjectChooserBase::EIteratorStatus Status = SelectedResult.ChooseMulti(Context, Callback);
			if (Status != FObjectChooserBase::EIteratorStatus::Continue)
			{
				bSetOutputs = true;
				// trigger all output columns to set their default output value
				for (const FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
				{
					const FChooserColumnBase& Column = ColumnData.Get<FChooserColumnBase>();
					Column.SetOutputs(Context, -1);
				}
			}
			if (Status == FObjectChooserBase::EIteratorStatus::Stop)
			{
				return FObjectChooserBase::EIteratorStatus::Stop;
			}
		}
	}
	else
	{
		// of the rows that passed all column filters, iterate through them calling the callback until it returns Stop
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
					TRACE_CHOOSER_EVALUATION(Chooser, Context, SelectedIndex);
				}
				if (Status == FObjectChooserBase::EIteratorStatus::Stop)
				{
					return FObjectChooserBase::EIteratorStatus::Stop;
				}
			}
		}
	}
	// If this is a nested chooser make sure the parent also sets the output values from the row that contained this chooser
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

void FEvaluateChooser::GetDebugName(FString& OutDebugName) const
{
	if (Chooser)
	{
		OutDebugName = Chooser.GetName();
	}
}


FNestedChooser::FNestedChooser()
{
}

UObject* FNestedChooser::ChooseObject(FChooserEvaluationContext& Context) const
{
	UObject* Result = nullptr;
	UChooserTable::EvaluateChooser(Context, Chooser, FObjectChooserIteratorCallback::CreateLambda([&Result](UObject* InResult)
	{
		Result = InResult;
		return FObjectChooserBase::EIteratorStatus::Stop;
	}));

	return Result;
}

FObjectChooserBase::EIteratorStatus FNestedChooser::ChooseMulti(FChooserEvaluationContext& Context, FObjectChooserIteratorCallback Callback) const
{
	return UChooserTable::EvaluateChooser(Context, Chooser, Callback);
}

void FNestedChooser::GetDebugName(FString& OutDebugName) const
{
	OutDebugName  = GetNameSafe(Chooser);
}