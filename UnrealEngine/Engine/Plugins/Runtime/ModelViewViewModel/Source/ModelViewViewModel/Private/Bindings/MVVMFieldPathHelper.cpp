// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MVVMFieldPathHelper.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Engine/Engine.h"
#include "FieldNotification/IFieldValueChanged.h"
#include "Misc/MemStack.h"
#include "MVVMSubsystem.h"
#include "Types/MVVMAvailableBinding.h"

#define LOCTEXT_NAMESPACE "MVVMFieldPathHelper"

namespace UE::MVVM::FieldPathHelper
{

namespace Private
{

static const FName NAME_BlueprintGetter = "BlueprintGetter";
static const FName NAME_BlueprintSetter = "BlueprintSetter";


TValueOrError<UStruct*, FText> FindContainer(const FProperty* Property)
{
	const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property);
	const FStructProperty* StructProperty = CastField<const FStructProperty>(Property);

	if (ObjectProperty)
	{
		return MakeValue(ObjectProperty->PropertyClass);
	}
	else if (StructProperty)
	{
		return MakeValue(StructProperty->Struct);
	}
	return MakeError(FText::Format(LOCTEXT("InvalidPropertyTypeInSourcePath", "Only object or struct properties can be used as source paths. '{0}' is a '{1}'."), FText::FromString(Property->GetName()), FText::FromString(Property->GetClass()->GetName())));
}


TValueOrError<FMVVMConstFieldVariant, FText> TransformWithAccessor(UStruct* CurrentContainer, FMVVMConstFieldVariant CurrentField, bool bForReading)
{
#if WITH_EDITORONLY_DATA
	if (bForReading)
	{
		if (!CurrentField.GetProperty()->HasGetter())
		{
			const FString& BlueprintGetter = CurrentField.GetProperty()->GetMetaData(Private::NAME_BlueprintGetter);
			if (!BlueprintGetter.IsEmpty())
			{
				FMVVMConstFieldVariant NewField = BindingHelper::FindFieldByName(CurrentContainer, FMVVMBindingName(*BlueprintGetter));
				if (NewField.IsFunction())
				{
					CurrentField = NewField;
				}
				else
				{
					return MakeError(FText::Format(LOCTEXT("BlueprintGetterNotFound", "The BlueprintGetter '{0}' could not be found on object '{1}'."), 
						FText::FromString(BlueprintGetter), 
						FText::FromString(CurrentContainer->GetName()))
					);
				}
			}
		}
	}
	else
	{
		if (!CurrentField.GetProperty()->HasSetter())
		{
			const FString& BlueprintSetter = CurrentField.GetProperty()->GetMetaData(Private::NAME_BlueprintSetter);
			if (!BlueprintSetter.IsEmpty())
			{
				FMVVMConstFieldVariant NewField = BindingHelper::FindFieldByName(CurrentContainer, FMVVMBindingName(*BlueprintSetter));
				if (NewField.IsFunction())
				{
					CurrentField = NewField;
				}
				else
				{
					return MakeError(FText::Format(LOCTEXT("BlueprintSetterNotFound", "The BlueprintSetter '{0}' could not be found on object '{1}'."), 
						FText::FromString(BlueprintSetter),
						FText::FromString(CurrentContainer->GetName()))
					);
				}
			}
		}
	}
#endif
	return MakeValue(CurrentField);
}

} // namespace


TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GenerateFieldPathList(const UClass* InFrom, FStringView InFieldPath, bool bForReading)
{
	if (InFrom == nullptr)
	{
		return MakeError(LOCTEXT("SourceClassInvalid", "The source class is invalid."));
	}
	if (InFieldPath.IsEmpty())
	{
		return MakeError(LOCTEXT("FieldPathEmpty", "The field path is empty."));
	}
	if (InFieldPath[InFieldPath.Len() - 1] == TEXT('.'))
	{
		return MakeError(LOCTEXT("FieldPathEndsInPeriod", "The field path cannot end with a '.' character."));
	}

	FMemMark Mark(FMemStack::Get());
	TArray<FMVVMConstFieldVariant, TMemStackAllocator<>> Result;

	const UStruct* CurrentContainer = InFrom;

	// Split the string into property or function names
	//ie. myvar.myfunction.myvar
	int32 FoundIndex = INDEX_NONE;
	while (InFieldPath.FindChar(TEXT('.'), FoundIndex))
	{
		FMVVMConstFieldVariant Field = BindingHelper::FindFieldByName(CurrentContainer, FMVVMBindingName(FName(FoundIndex, InFieldPath.GetData())));
		if (Field.IsEmpty())
		{
			return MakeError(FText::Format(LOCTEXT("FieldDoesNotExistInStruct", "The field '{0}' does not exist in the struct '{1}'."), 
				FText::FromName(FName(FoundIndex, InFieldPath.GetData())), 
				CurrentContainer ? FText::FromString(CurrentContainer->GetName()) : LOCTEXT("None", "<none>"))
			);
		}
		else if (Field.IsProperty())
		{
			TValueOrError<UStruct*, FText> FoundContainer = Private::FindContainer(Field.GetProperty());
			if (FoundContainer.HasError())
			{
				return MakeError(FoundContainer.StealError());
			}
			CurrentContainer = FoundContainer.GetValue();
		}
		
		if (Field.IsFunction())
		{
			const FProperty* ReturnProperty = BindingHelper::GetReturnProperty(Field.GetFunction());
			TValueOrError<UStruct*, FText> FoundContainer = Private::FindContainer(ReturnProperty);
			if (FoundContainer.HasError())
			{
				return MakeError(FoundContainer.StealError());
			}
			CurrentContainer = FoundContainer.GetValue();
		}

		InFieldPath = InFieldPath.RightChop(FoundIndex + 1);
		Result.Add(Field);
	}

	// The last field can be anything (that is what we are going to bind to)
	if (InFieldPath.Len() > 0)
	{
		FMVVMConstFieldVariant Field = BindingHelper::FindFieldByName(CurrentContainer, FMVVMBindingName(InFieldPath.GetData()));
		if (Field.IsEmpty())
		{
			return MakeError(FText::Format(LOCTEXT("FieldDoesNotExistInStruct", "The field '{0}' does not exist in the struct '{1}'."), 
				FText::FromStringView(InFieldPath), 
				CurrentContainer ? FText::FromString(CurrentContainer->GetName()) : LOCTEXT("None", "<none>"))
			);
		}

		Result.Add(Field);
	}

	return GenerateFieldPathList(Result, bForReading);
}


/**
 * Rules for reading:
 *		* Build path using Getter or BlueprintGetter if needed.
 *		* If the FProperty is a FStructProperty and a function was used, then the runtime may use dynamic memory instead of stack memory.
 * Rules for writing:
 *		* Build path using Getter or BlueprintGetter, the last element should use Setter or BlueprintSetter.
 *		* If one of the element in the path has a Setter or BlueprintSetter, then the path needs to stop there and be divided in 3 paths
 *			* ex: PropertyA.PropertyB.PropertyC.PropertyD.PropertyE and PropertyC has a Setter/BlueprintSetter
 *			* 1. To write: Property.Property.SetPropertyC()
 *			* 2. To read: PropertyA.PropertyB.PropertyC or PropertyA.PropertyB.GetPropertyC()
 *			* 3. To continue the reading: PropertyD.PropertyE
 *				PropertyD and PropertyE cannot have Getter/BlueprintGetter if they are FStructProperty
 *		* We can only have one Setter/BlueprintSetter in the path
 */
TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GenerateFieldPathList(TArrayView<const FMVVMConstFieldVariant> InFieldPath, bool bForSourceBinding)
{
	if (InFieldPath.Num() == 0)
	{
		return MakeError(LOCTEXT("FieldPathEmpty", "The field path is empty."));
	}

	TArray<FMVVMConstFieldVariant> Result;
	Result.Reserve(InFieldPath.Num());
	UStruct* CurrentContainer = InFieldPath[0].GetOwner();

	for (int32 Index = 0; Index < InFieldPath.Num(); ++Index)
	{
		bool bLastField = Index == InFieldPath.Num() - 1;
		FMVVMConstFieldVariant Field = InFieldPath[Index];

		if (Field.IsEmpty())
		{
			return MakeError(FText::Format(LOCTEXT("FieldIsEmpty", "The field at index '{0}' is empty."), Index));
		}

		if (CurrentContainer == nullptr)
		{
			return MakeError(FText::Format(LOCTEXT("FieldDoesNotExist", "The field '{0}' does not exist."), FText::FromName(Field.GetName())));
		}

		const bool bIsChild = Field.GetOwner()->IsChildOf(CurrentContainer);
		const bool bIsDownCast = CurrentContainer->IsChildOf(Field.GetOwner());
		if (!(bIsChild || bIsDownCast))
		{
			return MakeError(FText::Format(LOCTEXT("FieldDoesNotExistInStruct", "The field '{0}' does not exist in the struct '{1}'."), 
				FText::FromName(Field.GetName()), 
				FText::FromString(CurrentContainer->GetName()))
			);
		}
		
		if (Field.IsProperty())
		{
			TValueOrError<FMVVMConstFieldVariant, FText> TransformedField = Private::TransformWithAccessor(CurrentContainer, Field, (bForSourceBinding || !bLastField));
			if (TransformedField.HasError())
			{
				return MakeError(TransformedField.StealError());
			}
			Field = TransformedField.StealValue();
			check(!Field.IsEmpty());

			if (!bLastField && Field.IsProperty())
			{
				TValueOrError<UStruct*, FText> FoundContainer = Private::FindContainer(Field.GetProperty());
				if (FoundContainer.HasError())
				{
					return MakeError(FoundContainer.StealError());
				}
				CurrentContainer = FoundContainer.GetValue();
			}
		}

		if (!bLastField && Field.IsFunction())
		{
			const FProperty* ReturnProperty = BindingHelper::GetReturnProperty(Field.GetFunction());
			TValueOrError<UStruct*, FText> FoundContainer = Private::FindContainer(ReturnProperty);
			if (FoundContainer.HasError())
			{
				return MakeError(FoundContainer.StealError());
			}
			CurrentContainer = FoundContainer.GetValue();
		}

		Result.Add(Field);
	}

	return MakeValue(MoveTemp(Result));
}


TValueOrError<FParsedBindingInfo, FText> GetBindingInfoFromFieldPath(const UClass* InAccessor, TArrayView<const FMVVMConstFieldVariant> InFieldPath)
{
	if (InAccessor == nullptr)
	{
		return MakeError(LOCTEXT("AccessorClassNull", "The accessor class is null."));
	}
	if (InFieldPath.Num() == 0)
	{
		return MakeError(LOCTEXT("FieldPathEmpty", "The field path is empty."));
	}

	FParsedBindingInfo Result;
	FParsedBindingInfo::FFieldVariantArray LocalFullPath;
	bool bLookForNotifyFieldId = false;
	const UStruct* CurrentContainerType = InFieldPath[0].GetOwner();
	if (!InAccessor->IsChildOf(CurrentContainerType))
	{
		return MakeError(LOCTEXT("FieldPathDoesntStartWithAccessor", "Field path that doesn't start with the accessor is not supported."));
	}

	for (int32 Index = 0; Index < InFieldPath.Num(); ++Index)
	{
		bool bLastField = Index == InFieldPath.Num() - 1;
		FMVVMConstFieldVariant Field = InFieldPath[Index];

		if (Field.IsEmpty())
		{
			return MakeError(FText::Format(LOCTEXT("FieldIsEmpty", "The field at index '{0}' is empty."), Index));
		}

		if (CurrentContainerType == nullptr)
		{
			return MakeError(FText::Format(LOCTEXT("FieldDoesNotExist", "The field '{0}' does not exist."), 
				FText::FromName(Field.GetName()))
			);
		}

		const bool bIsChild = Field.GetOwner()->IsChildOf(CurrentContainerType);
		const bool bIsDownCast = CurrentContainerType->IsChildOf(Field.GetOwner());
		if (!(bIsChild || bIsDownCast))
		{
			return MakeError(FText::Format(LOCTEXT("FieldDoesNotExistInStruct", "The field '{0}' does not exist in the struct '{1}'."), 
				FText::FromName(Field.GetName()), 
				FText::FromString(CurrentContainerType->GetName()))
			);
		}

		const FProperty* FieldProperty = nullptr;
		if (Field.IsProperty())
		{
			FieldProperty = Field.GetProperty();
		}
		else if (Field.IsFunction() && Field.GetFunction())
		{
			FieldProperty = BindingHelper::GetReturnProperty(Field.GetFunction());
		}

		if (FieldProperty == nullptr)
		{
			return MakeError(FText::Format(LOCTEXT("FieldHasInvalidProperty", "The field '{0}' has an invalid property."), 
				FText::FromName(Field.GetName()))
			);
		}


		LocalFullPath.Add(Field);

		// Is field a viewmodel
		bool bSaveNotifyFieldInterfacePath = false;
		if (!bLastField)
		{
			if (const FObjectPropertyBase* FieldObjectProperty = CastField<const FObjectPropertyBase>(FieldProperty))
			{
				if (FieldObjectProperty->PropertyClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
				{
					Result.NotifyFieldClass = FieldObjectProperty->PropertyClass;
					Result.NotifyFieldId = FFieldNotificationId();
					Result.NotifyFieldInterfacePath = LocalFullPath;
					Result.ViewModelIndex = Index;
					bSaveNotifyFieldInterfacePath = true;
					bLookForNotifyFieldId = true;
				}
			}
		}

		if (!bSaveNotifyFieldInterfacePath)
		{
			if (bLookForNotifyFieldId && Result.NotifyFieldClass.Get() != nullptr)
			{
				FMVVMBindingName BindingName = FMVVMBindingName(Field.GetName());
				FMVVMAvailableBinding AvailableBinding = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBinding(Result.NotifyFieldClass.Get(), BindingName, InAccessor);
				if (AvailableBinding.IsValid())
				{
					Result.NotifyFieldId = FFieldNotificationId(AvailableBinding.GetBindingName().ToName());
				}
			}
			bLookForNotifyFieldId = false;
		}

		if (!bLastField)
		{
			TValueOrError<UStruct*, FText> FoundContainer = Private::FindContainer(FieldProperty);
			if (FoundContainer.HasError())
			{
				return MakeError(FoundContainer.StealError());
			}
			CurrentContainerType = FoundContainer.GetValue();
		}
	}

	if (Result.ViewModelIndex > 0)
	{
		TValueOrError<TArray<FMVVMConstFieldVariant>, FText> FinalPath = GenerateFieldPathList(Result.NotifyFieldInterfacePath, true);
		if (FinalPath.HasError())
		{
			return MakeError(FinalPath.StealError());
		}
		Result.NotifyFieldInterfacePath = FinalPath.StealValue();
	}

	return MakeValue(MoveTemp(Result));
}


FString ToString(TArrayView<const FMVVMFieldVariant> Fields)
{
	TStringBuilder<512> Builder;
	for (int32 Index = 0; Index < Fields.Num(); ++Index)
	{
		if (Index != 0)
		{
			Builder << TEXT(".");
		}
		Builder << Fields[Index].GetName();
	}
	return Builder.ToString();
}


FString ToString(TArrayView<const FMVVMConstFieldVariant> Fields)
{
	TStringBuilder<512> Builder;
	for (int32 Index = 0; Index < Fields.Num(); ++Index)
	{
		if (Index != 0)
		{
			Builder << TEXT(".");
		}
		Builder << Fields[Index].GetName();
	}
	return Builder.ToString();
}

FText ToText(TArrayView<const FMVVMFieldVariant> Fields)
{
	TArray<FText> FieldNames;
	FieldNames.Reserve(Fields.Num());

	for (const FMVVMFieldVariant& Field : Fields)
	{
		FieldNames.Add(FText::FromName(Field.GetName()));
	}

	return FText::Join(LOCTEXT("PropertyPathDelim", "."), FieldNames);
}

FText ToText(TArrayView<const FMVVMConstFieldVariant> Fields)
{
	TArray<FText> FieldNames;
	FieldNames.Reserve(Fields.Num());

	for (const FMVVMConstFieldVariant& Field : Fields)
	{
		FieldNames.Add(FText::FromName(Field.GetName()));
	}

	return FText::Join(LOCTEXT("PropertyPathDelim", "."), FieldNames);
}

TValueOrError<UObject*, void> EvaluateObjectProperty(const FFieldContext& InSource)
{
	if (InSource.GetObjectVariant().IsNull())
	{
		return MakeError();
	}

	const bool bIsProperty = InSource.GetFieldVariant().IsProperty();
	const FProperty* GetterType = bIsProperty ? InSource.GetFieldVariant().GetProperty() : BindingHelper::GetReturnProperty(InSource.GetFieldVariant().GetFunction());
	check(GetterType);

	const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(GetterType);
	if (ObjectProperty == nullptr)
	{
		return MakeError();
	}

	if (bIsProperty)
	{
		return MakeValue(ObjectProperty->GetObjectPropertyValue_InContainer(InSource.GetObjectVariant().GetData()));
	}
	else
	{
		check(InSource.GetObjectVariant().IsUObject());
		check(InSource.GetObjectVariant().GetUObject());

		UFunction* Function = InSource.GetFieldVariant().GetFunction();
		if (!InSource.GetObjectVariant().GetUObject()->GetClass()->IsChildOf(Function->GetOuterUClass()))
		{
			return MakeError();
		}

		void* DataPtr = FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment());
		ObjectProperty->InitializeValue(DataPtr);
		InSource.GetObjectVariant().GetUObject()->ProcessEvent(Function, DataPtr);
		UObject* Result = ObjectProperty->GetObjectPropertyValue_InContainer(DataPtr);
		ObjectProperty->DestroyValue(DataPtr);
		return MakeValue(Result);
	}
}

} // namespace

#undef LOCTEXT_NAMESPACE