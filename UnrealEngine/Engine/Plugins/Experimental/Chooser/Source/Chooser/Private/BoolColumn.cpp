// Copyright Epic Games, Inc. All Rights Reserved.
#include "BoolColumn.h"
#include "ChooserPropertyAccess.h"
#include "Chooser.h"

bool FBoolContextProperty::GetValue(FChooserEvaluationContext& Context, bool& OutResult) const
{
	
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;
	
	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			OutResult = *Property->ContainerPtrToValuePtr<bool>(Container);
			return true;
		}
		
	    if (const UClass* ClassType = Cast<const UClass>(StructType))
	    {
			if (UFunction* Function = ClassType->FindFunctionByName(Binding.PropertyBindingChain.Last()))
			{
				UObject* Object = reinterpret_cast<UObject*>(const_cast<void*>(Container));
				if (Function->IsNative())
				{
					FFrame Stack(Object, Function, nullptr, nullptr, Function->ChildProperties);
					Function->Invoke(Object, Stack, &OutResult);
				}
				else
				{
					Object->ProcessEvent(Function, &OutResult);
				}
			} 
		}
	}

	return false;
}

bool FBoolContextProperty::SetValue(FChooserEvaluationContext& Context, bool InValue) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;
	
	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (FBoolProperty* Property = FindFProperty<FBoolProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			// const cast is here just because ResolvePropertyChain expects a const void*&
			*Property->ContainerPtrToValuePtr<bool>(const_cast<void*>(Container)) = InValue;
			return true;
		}
	}

	return false;
}

FBoolColumn::FBoolColumn()
{
	InputValue.InitializeAs(FBoolContextProperty::StaticStruct());
}

void FBoolColumn::Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	if (InputValue.IsValid())
	{
		bool Result = false;
		InputValue.Get<FChooserParameterBoolBase>().GetValue(Context,Result);

	#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = Result;
		}
	#endif
		
		for (uint32 Index : IndexListIn)
		{
			if (RowValuesWithAny.Num() > (int)Index)
			{
				
				if (RowValuesWithAny[Index] == EBoolColumnCellValue::MatchAny || Result == static_cast<bool>(RowValuesWithAny[Index]))
				{
					IndexListOut.Push(Index);
				}
			}
		}
	}
	else
	{
		// passthrough fallback (behaves better during live editing)
		IndexListOut = IndexListIn;
	}
}