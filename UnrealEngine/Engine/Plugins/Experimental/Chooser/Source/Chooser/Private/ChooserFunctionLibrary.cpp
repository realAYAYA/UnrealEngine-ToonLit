// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChooserFunctionLibrary.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Chooser.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChooserFunctionLibrary)

UChooserFunctionLibrary::UChooserFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static FObjectChooserBase::EIteratorStatus StaticEvaluateChooser(const UObject* ContextObject, const UChooserTable* Chooser, FObjectChooserBase::FObjectChooserIteratorCallback Callback)
{
	// Fallback single context object version
	FChooserEvaluationContext Context(const_cast<UObject*>(ContextObject));
	return UChooserTable::EvaluateChooser(Context, Chooser, Callback);
}

UObject* UChooserFunctionLibrary::EvaluateChooser(const UObject* ContextObject, const UChooserTable* Chooser, TSubclassOf<UObject> ObjectClass)
{
	UObject* Result = nullptr;
	StaticEvaluateChooser(ContextObject, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result, ObjectClass](UObject* InResult)
	{
		if (InResult && (ObjectClass == nullptr || InResult->IsA(ObjectClass)))
		{
			Result = InResult;
			return FObjectChooserBase::EIteratorStatus::Stop;
		}
		return FObjectChooserBase::EIteratorStatus::Continue;
	}));

	return Result;
}

void UChooserFunctionLibrary::AddChooserObjectInput(FChooserEvaluationContext& Context, UObject* Object)
{
	Context.AddObjectParam(Object);
}

void UChooserFunctionLibrary::AddChooserStructInput(FChooserEvaluationContext& Inputs, int32 Value)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UChooserFunctionLibrary::execAddChooserStructInput)
{
	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	
	P_GET_STRUCT_REF(FChooserEvaluationContext, ChooserContext);
	
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* ValueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8* ValuePtr = Stack.MostRecentPropertyAddress;
	P_FINISH;
	
	if (!ValueProp || !ValuePtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			NSLOCTEXT("ChooserTable", "InstancedStruct_MakeInvalidValueWarning", "Failed to resolve the Value for Add Chooser Struct Input")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;
		ChooserContext.Params.Add(FStructView(ValueProp->Struct, ValuePtr));
		P_NATIVE_END;
	}
}

void UChooserFunctionLibrary::GetChooserStructOutput(FChooserEvaluationContext& Context, int index, int32& Value)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UChooserFunctionLibrary::execGetChooserStructOutput)
{
	P_GET_STRUCT_REF(FChooserEvaluationContext, Outputs);
	P_GET_PROPERTY(FIntProperty, Index);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	
	const FStructProperty* ValueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ValueProp || !ValuePtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			NSLOCTEXT("ChooserTable","GetChooserStructOutput_InvalidValueWarning", "Failed to resolve the Value for Get Chooser Struct Output")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;
		if (Outputs.Params.Num() > Index && Outputs.Params[Index].IsValid() && Outputs.Params[Index].GetScriptStruct()->IsChildOf(ValueProp->Struct))
		{
			ValueProp->Struct->CopyScriptStruct(ValuePtr, Outputs.Params[Index].GetMemory());
		}
		P_NATIVE_END;
	}
}

UObject* UChooserFunctionLibrary::EvaluateObjectChooserBase(FChooserEvaluationContext& Context, const FInstancedStruct& ObjectChooser, TSubclassOf<UObject> ObjectClass, bool bResultIsClass)
 {
 	UObject* Result = nullptr;

	if (const FObjectChooserBase* ObjectChooserPtr = ObjectChooser.GetPtr<FObjectChooserBase>())
	{
		ObjectChooserPtr->ChooseMulti(Context, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result, bResultIsClass, ObjectClass](UObject* InResult)
		{
			if (InResult)
			{
				bool bTypesMatch = true;

				if (ObjectClass && InResult)
				{
					if (bResultIsClass)
					{
						UClass* ResultClass =  Cast<UClass>(InResult);
						bTypesMatch = ResultClass && ResultClass->IsChildOf(ObjectClass);
					}
					else
					{
						bTypesMatch = InResult->IsA(ObjectClass);
					}
				}

				if (bTypesMatch)
				{
					Result = InResult;
					return FObjectChooserBase::EIteratorStatus::Stop;
				}
			}
			return FObjectChooserBase::EIteratorStatus::Continue;
		}));
	}
	
 	return Result;
 }


TArray<UObject*> UChooserFunctionLibrary::EvaluateObjectChooserBaseMulti(FChooserEvaluationContext& Context, const FInstancedStruct& ObjectChooser, TSubclassOf<UObject> ObjectClass, bool bResultIsClass)
{
 	TArray<UObject*> Result;

	if (const FObjectChooserBase* ObjectChooserPtr = ObjectChooser.GetPtr<FObjectChooserBase>())
	{
		ObjectChooserPtr->ChooseMulti(Context, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result, bResultIsClass, ObjectClass](UObject* InResult)
		{
			bool bTypesMatch = true;
            
			if (ObjectClass && InResult)
			{
				if (bResultIsClass)
				{
					UClass* ResultClass =  Cast<UClass>(InResult);
					bTypesMatch = ResultClass && ResultClass->IsChildOf(ObjectClass);
				}
				else
				{
					bTypesMatch = InResult->IsA(ObjectClass);
				}
			}
            
			if (bTypesMatch)
			{
				Result.Add(InResult);
			}
			// trigger output columns only on the first result
			return Result.Num() == 1 ? FObjectChooserBase::EIteratorStatus::ContinueWithOutputs : FObjectChooserBase::EIteratorStatus::Continue;
		}));
	}
	
 	return Result;
 }

FInstancedStruct UChooserFunctionLibrary::MakeEvaluateChooser(UChooserTable* Chooser)
{
	FInstancedStruct Struct;
	Struct.InitializeAs(FEvaluateChooser::StaticStruct());
	Struct.GetMutable<FEvaluateChooser>().Chooser = Chooser;
	return Struct;
}

FChooserEvaluationContext UChooserFunctionLibrary::MakeChooserEvaluationContext()
{
	FChooserEvaluationContext Context;
	return Context;
}

TArray<UObject*> UChooserFunctionLibrary::EvaluateChooserMulti(const UObject* ContextObject, const UChooserTable* Chooser, TSubclassOf<UObject> ObjectClass)
{
	TArray<UObject*> Result;
	StaticEvaluateChooser(ContextObject, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result, ObjectClass](UObject* InResult)
	{
		if (InResult && (ObjectClass == nullptr || InResult->IsA(ObjectClass)))
		{
			Result.Add(InResult);
		}
		// trigger output columns only on the first result
		return Result.Num() == 1 ? FObjectChooserBase::EIteratorStatus::ContinueWithOutputs : FObjectChooserBase::EIteratorStatus::Continue;
	}));

	for (int Index = 0; Index < Result.Num(); ++Index)
	{
		if (Result[Index] && !Result[Index]->IsA(ObjectClass))
		{
			Result[Index] = nullptr;
		}
	}

	return Result;
}