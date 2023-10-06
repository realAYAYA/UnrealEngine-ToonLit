// Copyright Epic Games, Inc. All Rights Reserved.
#include "FloatRangeColumn.h"

#include "ChooserPropertyAccess.h"

bool FFloatContextProperty::SetValue(FChooserEvaluationContext& Context, double InValue) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;
	
	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (FDoubleProperty* DoubleProperty = FindFProperty<FDoubleProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			// const cast is here just because ResolvePropertyChain expects a const void*&
			*DoubleProperty->ContainerPtrToValuePtr<double>(const_cast<void*>(Container)) = InValue;
			return true;
		}
		if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(StructType, Binding.PropertyBindingChain.Last()))
        {
			// const cast is here just because ResolvePropertyChain expects a const void*&
        	*FloatProperty->ContainerPtrToValuePtr<float>(const_cast<void*>(Container)) = InValue;
        	return true;
        }
	}

	return false;
}

bool FFloatContextProperty::GetValue(FChooserEvaluationContext& Context, double& OutResult) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;

	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (const FDoubleProperty* DoubleProperty = FindFProperty<FDoubleProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			OutResult = *DoubleProperty->ContainerPtrToValuePtr<double>(Container);
			return true;
		}
		
		if (const FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			OutResult = *FloatProperty->ContainerPtrToValuePtr<float>(Container);
			return true;
		}

	    if (const UClass* ClassType = Cast<const UClass>(StructType))
	    {
			if (UFunction* Function = ClassType->FindFunctionByName(Binding.PropertyBindingChain.Last()))
			{
				const bool bReturnsDouble = CastField<FDoubleProperty>(Function->GetReturnProperty()) != nullptr;
					
				UObject* Object = reinterpret_cast<UObject*>(const_cast<void*>(Container));
				if (Function->IsNative())
				{
					FFrame Stack(Object, Function, nullptr, nullptr, Function->ChildProperties);
					if (bReturnsDouble)
					{
						Function->Invoke(Object, Stack, &OutResult);
					}
					else
					{
						float result = 0;
						Function->Invoke(Object, Stack, &result);
						OutResult = result;
					}
				}
				else
				{
					if (bReturnsDouble)
					{
						Object->ProcessEvent(Function, &OutResult);
					}
					else
					{
						float result = 0;
						Object->ProcessEvent(Function, &result);
						OutResult = result;
					}
				}
				return true;
			} 
		}
	}

	return false;
}

FFloatRangeColumn::FFloatRangeColumn()
{
	InputValue.InitializeAs(FFloatContextProperty::StaticStruct());
}

void FFloatRangeColumn::Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	if (InputValue.IsValid())
	{
		double Result = 0.0f;
		InputValue.Get<FChooserParameterFloatBase>().GetValue(Context, Result);

#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = Result;
		}
#endif

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
	else
	{
		// passthrough fallback (behaves better during live editing)
		IndexListOut = IndexListIn;
	}
}