// Copyright Epic Games, Inc. All Rights Reserved.
#include "ObjectColumn.h"
#include "ChooserPropertyAccess.h"

#if WITH_EDITOR
	#include "IPropertyAccessEditor.h"
#endif

bool FObjectContextProperty::GetValue(FChooserEvaluationContext& Context, FSoftObjectPath& OutResult) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;

	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (const FObjectPropertyBase* ObjectProperty = FindFProperty<FObjectPropertyBase>(StructType, Binding.PropertyBindingChain.Last()))
		{
			// if the property is a soft object property, get the path directly
			if (ObjectProperty->IsA<FSoftObjectProperty>())
			{
				const FSoftObjectPtr& SoftObjectPtr = *ObjectProperty->ContainerPtrToValuePtr<FSoftObjectPtr>(Container);
				OutResult = SoftObjectPtr.ToSoftObjectPath();
				return true;
			}
			
			// otherwise get the value from the object property and convert to a soft object path
			const UObject* LoadedObject = ObjectProperty->GetObjectPropertyValue_InContainer(Container);
			OutResult = LoadedObject;
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR

void FObjectContextProperty::SetBinding(const TArray<FBindingChainElement>& InBindingChain)
{
	const UClass* PreviousClass = Binding.AllowedClass;
	Binding.AllowedClass = nullptr;

	UE::Chooser::CopyPropertyChain(InBindingChain, Binding);

	const FField* Field = InBindingChain.Last().Field.ToField();
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Field))
	{
		Binding.AllowedClass = ObjectProperty->PropertyClass;
	}
}

#endif // WITH_EDITOR

FObjectColumn::FObjectColumn()
{
#if WITH_EDITOR
	InputValue.InitializeAs(FObjectContextProperty::StaticStruct());
#endif
}

bool FChooserObjectRowData::Evaluate(const FSoftObjectPath& LeftHandSide) const
{
	switch (Comparison)
	{
		case EObjectColumnCellValueComparison::MatchEqual:
			return LeftHandSide == Value.ToSoftObjectPath();

		case EObjectColumnCellValueComparison::MatchNotEqual:
			return LeftHandSide != Value.ToSoftObjectPath();

		case EObjectColumnCellValueComparison::MatchAny:
			return true;

		default:
			return false;
	}
}

void FObjectColumn::Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	FSoftObjectPath Result;
	if (InputValue.IsValid() &&
		InputValue.Get<FChooserParameterObjectBase>().GetValue(Context, Result))
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
				const FChooserObjectRowData& RowValue = RowValues[Index];
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
