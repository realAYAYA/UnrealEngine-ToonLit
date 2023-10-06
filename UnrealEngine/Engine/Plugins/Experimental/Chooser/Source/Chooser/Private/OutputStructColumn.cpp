// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputStructColumn.h"
#include "ChooserPropertyAccess.h"
#include "OutputStructColumn.h"

#if WITH_EDITOR
	#include "IPropertyAccessEditor.h"
#endif

bool FStructContextProperty::SetValue(FChooserEvaluationContext& Context, const FInstancedStruct& InValue) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;

	if (Binding.PropertyBindingChain.IsEmpty())
	{
		if(Context.Params.IsValidIndex((Binding.ContextIndex)))
		{
			// directly bound to context struct
			if (Context.Params[Binding.ContextIndex].GetScriptStruct() == InValue.GetScriptStruct())
			{
				void* TargetData = Context.Params[Binding.ContextIndex].GetMutableMemory();
				InValue.GetScriptStruct()->CopyScriptStruct(TargetData, InValue.GetMemory());
			}
		}
	}
	else if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (FStructProperty* Property = FindFProperty<FStructProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			// const cast is here just because ResolvePropertyChain expects a const void*&
			void* TargetData = Property->ContainerPtrToValuePtr<void>(const_cast<void*>(Container));
			
			if (Property->Struct == InValue.GetScriptStruct())
			{
				Property->Struct->CopyScriptStruct(TargetData, InValue.GetMemory());
			}

			return true;
		}
	}

	return false;
}

#if WITH_EDITOR

void FStructContextProperty::SetBinding(const UObject* OuterObject, const TArray<FBindingChainElement>& InBindingChain)
{
	Binding.StructType = nullptr;

	UE::Chooser::CopyPropertyChain(InBindingChain, Binding);

	if (Binding.PropertyBindingChain.Num() == 0)
	{
		// binding directly to context struct, get struct type from there
	    if (const IHasContextClass* HasContextClass = Cast<IHasContextClass>(OuterObject))
	    {
	    	TConstArrayView<FInstancedStruct> ContextData = HasContextClass->GetContextData();
	    	if (ContextData.IsValidIndex(Binding.ContextIndex))
		    {
	    		if (const FContextObjectTypeStruct* StructContextData = ContextData[Binding.ContextIndex].GetPtr<FContextObjectTypeStruct>())
	    		{
					Binding.StructType = StructContextData->Struct;
	    			Binding.DisplayName = StructContextData->Struct->GetAuthoredName();
	    		}
		    }
	    }
	}
	else
	{
		const FField* Field = InBindingChain.Last().Field.ToField();
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Field))
		{
			Binding.StructType = StructProperty->Struct;
			Binding.DisplayName = StructProperty->GetAuthoredName();
			if (InBindingChain.Num() > 2)
			{
				// add the parent struct name to the display name if there is one
				
				if (const FField* NextToLastField = InBindingChain[InBindingChain.Num()-2].Field.ToField())
				{
					Binding.DisplayName = NextToLastField->GetAuthoredName() + "." + Binding.DisplayName;
				}
			}
		}
	}
}

void FOutputStructColumn::StructTypeChanged()
{
	if (InputValue.IsValid())
	{
		const UScriptStruct* Struct = InputValue.Get<FChooserParameterStructBase>().GetStructType();

		if (DefaultRowValue.GetScriptStruct() != Struct)
		{
			DefaultRowValue.InitializeAs(Struct);
		}

		for (FInstancedStruct& RowValue : RowValues)
		{
			if (RowValue.GetScriptStruct() != Struct)
			{
				RowValue.InitializeAs(Struct);
			}
		}
	}
}

#endif // WITH_EDITOR

FOutputStructColumn::FOutputStructColumn()
{
	InputValue.InitializeAs(FStructContextProperty::StaticStruct());
}

void FOutputStructColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		InputValue.Get<FChooserParameterStructBase>().SetValue(Context, RowValues[RowIndex]);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
