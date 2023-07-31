// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MVVMCompiledBindingLibrary.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/PropertyAccessUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMCompiledBindingLibrary)

/**
 *
 */
FProperty* FMVVMVCompiledFields::GetProperty(FName PropertyName) const
{
	return PropertyAccessUtil::FindPropertyByName(PropertyName, ClassOrScriptStruct);
}


UFunction* FMVVMVCompiledFields::GetFunction(FName FunctionName) const
{
	// ScriptStruct should not have UFunction
	return CastChecked<UClass>(ClassOrScriptStruct)->FindFunctionByName(FunctionName);
}


UE::FieldNotification::FFieldId FMVVMVCompiledFields::GetFieldId(FName FieldName) const
{
	// ScriptStruct can't implements UNotifyFieldValueChanged
	UClass* Class = CastChecked<UClass>(ClassOrScriptStruct);
	check(Class->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()));

	TScriptInterface<INotifyFieldValueChanged> Interface = Class->GetDefaultObject();
	return Interface->GetFieldNotificationDescriptor().GetField(Class, FieldName);
}


/**
 *
 */
FMVVMCompiledBindingLibrary::FMVVMCompiledBindingLibrary()
#if WITH_EDITORONLY_DATA
	: CompiledBindingLibraryId(FGuid::NewGuid())
#endif
{

}


void FMVVMCompiledBindingLibrary::Load()
{
	ensureAlwaysMsgf(LoadedProperties.Num() == 0, TEXT("The binding library was loaded more than once."));
	ensureAlwaysMsgf(LoadedFunctions.Num() == 0, TEXT("The binding library was loaded more than once."));
	ensureAlwaysMsgf(LoadedFieldIds.Num() == 0, TEXT("The binding library was loaded more than once."));

	for (const FMVVMVCompiledFields& Field : CompiledFields)
	{
		{
			const int32 NumberOfProperties = Field.GetPropertyNum();
			for (int32 Index = 0; Index < NumberOfProperties; ++Index)
			{
				FName FieldName = Field.GetPropertyName(CompiledFieldNames, Index);
				FProperty* LoadedProperty = Field.GetProperty(FieldName);
				LoadedProperties.Add(LoadedProperty); // add it even if none to keep the index valid
				ensureAlwaysMsgf(LoadedProperty != nullptr, TEXT("The property '%s:%s' could not be loaded."), (Field.GetStruct() ? *Field.GetStruct()->GetName() : TEXT("None")), *FieldName.ToString());
			}
		}
		{
			const int32 NumberOfFunctions = Field.GetFunctionNum();
			for (int32 Index = 0; Index < NumberOfFunctions; ++Index)
			{
				FName FieldName = Field.GetFunctionName(CompiledFieldNames, Index);
				UFunction* LoadedFunction = Field.GetFunction(FieldName);
				LoadedFunctions.Add(LoadedFunction); // add it even if none to keep the index valid
				ensureAlwaysMsgf(LoadedFunction != nullptr, TEXT("The function '%s:%s' could not be loaded."), (Field.GetStruct() ? *Field.GetStruct()->GetName() : TEXT("None")), *FieldName.ToString());
			}
		}
		{
			const int32 NumberOfFieldIds = Field.GetFieldIdNum();
			for (int32 Index = 0; Index < NumberOfFieldIds; ++Index)
			{
				FName FieldName = Field.GetFieldIdName(CompiledFieldNames, Index);
				UE::FieldNotification::FFieldId LoadedFieldId = Field.GetFieldId(FieldName);
				LoadedFieldIds.Add(LoadedFieldId); // add it even if none to keep the index valid
				ensureAlwaysMsgf(LoadedFieldId.IsValid(), TEXT("The field id '%s:%s' could not be loaded."), (Field.GetStruct() ? *Field.GetStruct()->GetName() : TEXT("None")), *FieldName.ToString());
			}
		}
	}

#if !WITH_EDITOR
	CompiledFieldNames.Reset(); // Once loaded, we do not need to keep that information.
	CompiledFields.Reset();
#endif
}


TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> FMVVMCompiledBindingLibrary::Execute(UObject* InExecutionSource, const FMVVMVCompiledBinding& InBinding, EConversionFunctionType InFunctionType) const
{
	check(InExecutionSource);

	return ExecuteImpl(InExecutionSource, InBinding, nullptr, InFunctionType);
}


TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> FMVVMCompiledBindingLibrary::ExecuteWithSource(UObject* InExecutionSource, const FMVVMVCompiledBinding& InBinding, UObject* InSource) const
{
	check(InExecutionSource);
	check(InSource);

	return ExecuteImpl(InExecutionSource, InBinding, InSource, EConversionFunctionType::Simple);
}


TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> FMVVMCompiledBindingLibrary::ExecuteImpl(UObject* InExecutionSource, const FMVVMVCompiledBinding& InBinding, UObject* InSource, EConversionFunctionType InFunctionType) const
{
#if WITH_EDITORONLY_DATA
	const bool bIsValidBinding = InBinding.CompiledBindingLibraryId == CompiledBindingLibraryId;
	if (!bIsValidBinding)
	{
		ensureAlwaysMsgf(false, TEXT("The binding is from a different library."));
		return MakeError(EExecutionFailingReason::IncompatibleLibrary);
	}
#endif

	UE::MVVM::FFieldContext ValidSource;
	if (InFunctionType != EConversionFunctionType::Complex)
	{
		if (InSource)
		{
#if DO_CHECK
			TValueOrError<UE::MVVM::FFieldContext, void> TestSource = EvaluateFieldPath(InExecutionSource, InBinding.SourceFieldPath);
			if (TestSource.HasError())
			{
				ensureAlwaysMsgf(false, TEXT("The source was provided but it was not able to fetch it."));
				return MakeError(EExecutionFailingReason::InvalidSource);
			}
			if (!TestSource.GetValue().GetObjectVariant().IsUObject())
			{
				ensureAlwaysMsgf(false, TEXT("The source was provided as a UObject but the fetched source is not a UObject."));
				return MakeError(EExecutionFailingReason::InvalidSource);
			}
			if (InSource != TestSource.GetValue().GetObjectVariant().GetUObject())
			{
				ensureAlwaysMsgf(false, TEXT("The source was provided and it's not the same as the fetched source."));
				return MakeError(EExecutionFailingReason::InvalidSource);
			}
			checkf(TestSource.GetValue().GetObjectVariant().GetData() != nullptr, TEXT("Tested in EvaluateFieldPath"));
#endif

			TValueOrError<UE::MVVM::FMVVMFieldVariant, void> FinalPath = GetFinalFieldFromPathImpl(InBinding.SourceFieldPath);
			if (FinalPath.HasError())
			{
				return MakeError(EExecutionFailingReason::InvalidSource);
			}
			ValidSource = UE::MVVM::FFieldContext(InSource, FinalPath.StealValue());
		}
		else
		{
			TValueOrError<UE::MVVM::FFieldContext, void> FoundSource = EvaluateFieldPath(InExecutionSource, InBinding.SourceFieldPath);
			if (FoundSource.HasError())
			{
				return MakeError(EExecutionFailingReason::InvalidSource);
			}
			checkf(FoundSource.GetValue().GetObjectVariant().GetData() != nullptr, TEXT("Tested in EvaluateFieldPath"));

			ValidSource = FoundSource.StealValue();
		}
	}

	TValueOrError<UE::MVVM::FFieldContext, void> FoundDestination = EvaluateFieldPath(InExecutionSource, InBinding.DestinationFieldPath);
	if (FoundDestination.HasError())
	{
		return MakeError(EExecutionFailingReason::InvalidDestination);
	}
	checkf(FoundDestination.GetValue().GetObjectVariant().GetData() != nullptr, TEXT("Tested in EvaluateFieldPath"));

	UE::MVVM::FFunctionContext ConversionFunction;
	if (InBinding.ConversionFunctionFieldPath.IsValid())
	{
		TValueOrError<UE::MVVM::FFieldContext, void> FoundConversionFunction = EvaluateFieldPath(InExecutionSource, InBinding.ConversionFunctionFieldPath);
		if (FoundConversionFunction.HasError() || !FoundConversionFunction.GetValue().GetFieldVariant().IsFunction())
		{
			return MakeError(EExecutionFailingReason::InvalidConversionFunction);
		}

		if (FoundConversionFunction.GetValue().GetFieldVariant().GetFunction()->HasAllFunctionFlags(FUNC_Static))
		{
			ConversionFunction = UE::MVVM::FFunctionContext::MakeStaticFunction(FoundConversionFunction.GetValue().GetFieldVariant().GetFunction());
		}
		else if (!FoundConversionFunction.GetValue().GetObjectVariant().IsUObject())
		{
			return MakeError(EExecutionFailingReason::InvalidConversionFunction);
		}
		else
		{
			ConversionFunction = UE::MVVM::FFunctionContext(FoundConversionFunction.GetValue().GetObjectVariant().GetUObject(), FoundConversionFunction.GetValue().GetFieldVariant().GetFunction());
		}
	}

	return ExecuteImpl(ValidSource, FoundDestination.GetValue(), ConversionFunction, InFunctionType);
}


TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> FMVVMCompiledBindingLibrary::ExecuteImpl(UE::MVVM::FFieldContext& Source, UE::MVVM::FFieldContext& Destination, UE::MVVM::FFunctionContext& ConversionFunction, EConversionFunctionType FunctionType) const
{
	if (FunctionType != EConversionFunctionType::Complex)
	{
		check(!Source.GetObjectVariant().IsEmpty());
		check(!Source.GetObjectVariant().IsNull());
		check(!Source.GetFieldVariant().IsEmpty());
		if (!Source.GetObjectVariant().GetOwner()->IsChildOf(Source.GetFieldVariant().GetOwner()))
		{
			return MakeError(EExecutionFailingReason::InvalidCast);
		}
	}
	check(!Destination.GetObjectVariant().IsEmpty());
	check(!Destination.GetObjectVariant().IsNull());
	check(!Destination.GetFieldVariant().IsEmpty());
	if (!Destination.GetObjectVariant().GetOwner()->IsChildOf(Destination.GetFieldVariant().GetOwner()))
	{
		return MakeError(EExecutionFailingReason::InvalidCast);
	}

	if (ConversionFunction.GetFunction())
	{
		if (FunctionType == EConversionFunctionType::Complex)
		{
			UE::MVVM::BindingHelper::ExecuteBinding_NoCheck(Destination, ConversionFunction);
		}
		else
		{
			UE::MVVM::BindingHelper::ExecuteBinding_NoCheck(Source, Destination, ConversionFunction);
		}
	}
	else
	{
		UE::MVVM::BindingHelper::ExecuteBinding_NoCheck(Source, Destination);
	}

	return MakeValue();
}


namespace UE::MVVM::Private
{
	UObject* EvaluateFieldPathObject(UObject* Container, UFunction* Function, const FObjectPropertyBase* ReturnObjectProperty)
	{
		void* DataPtr = FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment());
		ReturnObjectProperty->InitializeValue(DataPtr);
		Container->ProcessEvent(Function, DataPtr);
		UObject* NewContainer = ReturnObjectProperty->GetObjectPropertyValue(DataPtr);
		ReturnObjectProperty->DestroyValue(DataPtr);
		return NewContainer;
	}
}


TValueOrError<UE::MVVM::FFieldContext, void> FMVVMCompiledBindingLibrary::EvaluateFieldPath(UObject* InExecutionSource, const FMVVMVCompiledFieldPath& InFieldPath) const
{
	if (InExecutionSource == nullptr)
	{
		return MakeError();
	}

#if WITH_EDITORONLY_DATA
	const bool bIsValidBinding = InFieldPath.CompiledBindingLibraryId == CompiledBindingLibraryId;
	if (!bIsValidBinding)
	{
		ensureAlwaysMsgf(false, TEXT("The binding is from a different library."));
		return MakeError();
	}
#endif

	if (!InFieldPath.IsValid())
	{
		return MakeError();
	}

	UE::MVVM::FObjectVariant CurrentContainer = UE::MVVM::FObjectVariant(InExecutionSource);
	const int32 IterationMax = InFieldPath.StartIndex + InFieldPath.Num - 1;
	checkf(IterationMax < FieldPaths.Num(), TEXT("The number of field is bigger than the number of field that was compiled."));
	for (int32 Index = InFieldPath.StartIndex; Index < IterationMax; ++Index)
	{
		if (CurrentContainer.IsNull())
		{
			return MakeError();
		}

		check(FieldPaths.IsValidIndex(Index));
		FMVVMCompiledLoadedPropertyOrFunctionIndex PathIndex = FieldPaths[Index];
		if (PathIndex.bIsProperty)
		{
			check(LoadedProperties.IsValidIndex(PathIndex.Index));
			const FProperty* Property = LoadedProperties[PathIndex.Index];
			if (!Property)
			{
				return MakeError();
			}

			if (PathIndex.bIsObjectProperty)
			{
				const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property);
				UObject* NewContainer = ObjectProperty->GetObjectPropertyValue_InContainer(CurrentContainer.GetData()); // this skip any Getter
				CurrentContainer.SetUObject(NewContainer);
			}
			else if (PathIndex.bIsScriptStructProperty)
			{
				const FStructProperty* StrutProperty = CastFieldChecked<const FStructProperty>(Property);
				if (StrutProperty->HasGetter())
				{
					ensureAlwaysMsgf(false, TEXT("This is not supported since it would create a temporary structure and we would not able to return a valid container from that structure."));
					return MakeError();
				}

				CurrentContainer.SetUScriptStruct(StrutProperty->Struct, StrutProperty->ContainerPtrToValuePtr<void>(CurrentContainer.GetData()));
			}
			else
			{
				return MakeError();
			}
		}
		else
		{
			check(LoadedFunctions.IsValidIndex(PathIndex.Index));
			UFunction* Function = LoadedFunctions[PathIndex.Index];
			if (!Function)
			{
				return MakeError();
			}

			if (PathIndex.bIsObjectProperty)
			{
				const FObjectPropertyBase* ReturnObjectProperty = CastFieldChecked<const FObjectPropertyBase>(UE::MVVM::BindingHelper::GetReturnProperty(Function));
				check(ReturnObjectProperty);
				check(Function->NumParms == 1);
				
				if (!CurrentContainer.IsUObject())
				{
					return MakeError();
				}

				CurrentContainer.SetUObject(UE::MVVM::Private::EvaluateFieldPathObject(CurrentContainer.GetUObject(), Function, ReturnObjectProperty));
			}
			else if (PathIndex.bIsScriptStructProperty)
			{
				ensureAlwaysMsgf(false, TEXT("bIsScriptStructProperty is not supported since it would create a temporary structure and we would not able to return a valid container from that structure."));
				return MakeError();
			}
			else
			{
				return MakeError();
			}
		}
	}

	if (CurrentContainer.IsNull())
	{
		return MakeError();
	}

	TValueOrError<UE::MVVM::FMVVMFieldVariant, void> FinalPath = GetFinalFieldFromPathImpl(InFieldPath);
	if (FinalPath.HasError())
	{
		return MakeError();
	}
	return MakeValue(MoveTemp(CurrentContainer), FinalPath.StealValue());
}


TValueOrError<UE::MVVM::FMVVMFieldVariant, void> FMVVMCompiledBindingLibrary::GetFinalFieldFromPathImpl(const FMVVMVCompiledFieldPath& InFieldPath) const
{
	const int32 IterationMax = InFieldPath.StartIndex + InFieldPath.Num - 1;

	UE::MVVM::FMVVMFieldVariant FinalField;
	check(FieldPaths.IsValidIndex(IterationMax));
	FMVVMCompiledLoadedPropertyOrFunctionIndex FinalPathIndex = FieldPaths[IterationMax];
	if (FinalPathIndex.bIsProperty)
	{
		check(LoadedProperties.IsValidIndex(FinalPathIndex.Index));
		FProperty* Property = LoadedProperties[FinalPathIndex.Index];
		if (!Property)
		{
			return MakeError();
		}
		FinalField.SetProperty(Property);
	}
	else
	{
		check(LoadedFunctions.IsValidIndex(FinalPathIndex.Index));
		UFunction* Function = LoadedFunctions[FinalPathIndex.Index];
		if (!Function)
		{
			return MakeError();
		}
		FinalField.SetFunction(Function);
	}

	return MakeValue(MoveTemp(FinalField));
}


TValueOrError<FString, FString> FMVVMCompiledBindingLibrary::FieldPathToString(const FMVVMVCompiledFieldPath& FieldPath) const
{
#if WITH_EDITORONLY_DATA
	const bool bIsValidBinding = FieldPath.CompiledBindingLibraryId == CompiledBindingLibraryId;
	if (!bIsValidBinding)
	{
		ensureAlwaysMsgf(false, TEXT("The binding is from a different library."));
		return MakeError<FString>(TEXT("The binding is from a different library."));
	}
#endif

	TStringBuilder<512> StringBuilder;
	const int32 IterationMax = FieldPath.StartIndex + FieldPath.Num;
	checkf(IterationMax < FieldPaths.Num(), TEXT("The number of field is bigger than the number of field that was compiled."));
	for (int32 Index = FieldPath.StartIndex; Index < IterationMax; ++Index)
	{
		if (StringBuilder.Len() > 0)
		{
			StringBuilder.AppendChar(TEXT('.'));
		}

		check(FieldPaths.IsValidIndex(Index));
		FMVVMCompiledLoadedPropertyOrFunctionIndex PathIndex = FieldPaths[Index];
		if (PathIndex.bIsProperty)
		{
			check(LoadedProperties.IsValidIndex(PathIndex.Index));
			const FProperty* Property = LoadedProperties[PathIndex.Index];
			if (!Property)
			{
				return MakeError<FString>(StringBuilder.ToString());
			}
		}
		else
		{
			check(LoadedFunctions.IsValidIndex(PathIndex.Index));
			UFunction* Function = LoadedFunctions[PathIndex.Index];
			if (!Function)
			{
				return MakeError<FString>(StringBuilder.ToString());
			}
		}
	}
	return MakeValue<FString>(StringBuilder.ToString());
}


TValueOrError<UE::FieldNotification::FFieldId, void> FMVVMCompiledBindingLibrary::GetFieldId(const FMVVMVCompiledFieldId& InFieldId) const
{
#if WITH_EDITORONLY_DATA
	const bool bIsValidBinding = InFieldId.CompiledBindingLibraryId == CompiledBindingLibraryId;
	if (!bIsValidBinding)
	{
		ensureAlwaysMsgf(false, TEXT("The binding is from a different library."));
		return MakeError<>();
	}
#endif

	check(LoadedFieldIds.IsValidIndex(InFieldId.FieldIdIndex));
	return MakeValue(LoadedFieldIds[InFieldId.FieldIdIndex]);
}

