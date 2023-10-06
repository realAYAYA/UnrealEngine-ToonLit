// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintPin.h"

#include "Bindings/MVVMConversionFunctionHelper.h"
#include "EdGraph/EdGraphPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintPin)

FMVVMBlueprintPin FMVVMBlueprintPin::CreateFromPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	FMVVMBlueprintPin Result;
	Result.PinName = Pin->PinName;
	Result.Path = UE::MVVM::ConversionFunctionHelper::GetPropertyPathForPin(Blueprint, Pin, true);
	Result.DefaultObject = Pin->DefaultObject;
	Result.DefaultString = Pin->DefaultValue;
	Result.DefaultText = Pin->DefaultTextValue;
	return Result;
}

void FMVVMBlueprintPin::SetDefaultValue(UObject* Value)
{
	Reset();
	DefaultObject = Value;
}

void FMVVMBlueprintPin::SetDefaultValue(const FText& Value)
{
	Reset();
	DefaultText = Value;
}

void FMVVMBlueprintPin::SetDefaultValue(const FString& Value)
{
	Reset();
	DefaultString = Value;
}

void FMVVMBlueprintPin::SetPath(const FMVVMBlueprintPropertyPath& Value)
{
	Reset();
	Path = Value;
}

FString FMVVMBlueprintPin::GetValueAsString(const UClass* SelfContext) const
{
	if (!Path.IsEmpty())
	{
		return Path.GetPropertyPath(SelfContext);
	}
	else if (DefaultObject)
	{
		return DefaultObject.GetPathName();
	}
	else if (!DefaultText.IsEmpty())
	{
		FString TextAsString;
		FTextStringHelper::WriteToBuffer(TextAsString, DefaultText);
		return TextAsString;
	}
	else
	{
		return DefaultString;
	}
}

void FMVVMBlueprintPin::CopyTo(const UBlueprint* Blueprint, UEdGraphPin* Pin) const
{
	Pin->DefaultObject = DefaultObject;
	Pin->DefaultValue = DefaultString;
	Pin->DefaultTextValue = DefaultText;

	UE::MVVM::ConversionFunctionHelper::SetPropertyPathForPin(Blueprint, Path, Pin);
}

void FMVVMBlueprintPin::Reset()
{
	Path = FMVVMBlueprintPropertyPath();
	DefaultString.Empty();
	DefaultText = FText();
	DefaultObject = nullptr;
}
