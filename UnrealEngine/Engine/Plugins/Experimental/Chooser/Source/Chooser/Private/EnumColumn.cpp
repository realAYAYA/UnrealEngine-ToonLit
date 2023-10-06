// Copyright Epic Games, Inc. All Rights Reserved.
#include "EnumColumn.h"
#include "ChooserPropertyAccess.h"

#if WITH_EDITOR
#include "IPropertyAccessEditor.h"
#endif

bool FEnumContextProperty::GetValue(FChooserEvaluationContext& Context, uint8& OutResult) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;

	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (const FEnumProperty* EnumProperty = FindFProperty<FEnumProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			OutResult = *EnumProperty->ContainerPtrToValuePtr<uint8>(Container);
			return true;
		}

		if (const FByteProperty* ByteProperty = FindFProperty<FByteProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			if (ByteProperty->IsEnum())
			{
				OutResult = *ByteProperty->ContainerPtrToValuePtr<uint8>(Container);
				return true;
			}
		}
	}

	return false;
}

bool FEnumContextProperty::SetValue(FChooserEvaluationContext& Context, uint8 InValue) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;

	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (const FEnumProperty* EnumProperty = FindFProperty<FEnumProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			*EnumProperty->ContainerPtrToValuePtr<uint8>(const_cast<void*>(Container)) = InValue;
			return true;
		}

		if (const FByteProperty* ByteProperty = FindFProperty<FByteProperty>(StructType, Binding.PropertyBindingChain.Last()))
		{
			if (ByteProperty->IsEnum())
			{
				*ByteProperty->ContainerPtrToValuePtr<uint8>(const_cast<void*>(Container)) = InValue;
				return true;
			}
		}
	}

	return false;
}

#if WITH_EDITOR

void FEnumContextProperty::SetBinding(const TArray<FBindingChainElement>& InBindingChain)
{
	const UEnum* PreviousEnum = Binding.Enum;
	Binding.Enum = nullptr;

	UE::Chooser::CopyPropertyChain(InBindingChain, Binding);

	const FField* Field = InBindingChain.Last().Field.ToField();
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Field))
	{
		Binding.Enum = EnumProperty->GetEnum();
	}
	else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Field))
	{
		Binding.Enum = ByteProperty->Enum;
	}
}

#endif // WITH_EDITOR

FEnumColumn::FEnumColumn()
{
#if WITH_EDITOR
	InputValue.InitializeAs(FEnumContextProperty::StaticStruct());
#endif
}

bool FChooserEnumRowData::Evaluate(const uint8 LeftHandSide) const
{
	bool Equal = LeftHandSide == Value;
	return Equal ^ CompareNotEqual;
}

void FEnumColumn::Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	uint8 Result = 0;
	if (InputValue.IsValid() &&
		InputValue.Get<FChooserParameterEnumBase>().GetValue(Context, Result))
	{
#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = Result;
		}
#endif
		
		for (const uint32 Index : IndexListIn)
		{
			if (RowValues.IsValidIndex(Index))
			{
				const FChooserEnumRowData& RowValue = RowValues[Index];
				if (RowValue.Evaluate(Result))
				{
					IndexListOut.Emplace(Index);
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