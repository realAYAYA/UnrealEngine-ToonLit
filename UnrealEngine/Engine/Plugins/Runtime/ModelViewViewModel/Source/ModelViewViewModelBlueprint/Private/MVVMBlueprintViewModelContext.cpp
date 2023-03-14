// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewModelContext.h"

#include "FieldNotification/IFieldValueChanged.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintViewModelContext)

FMVVMBlueprintViewModelContext::FMVVMBlueprintViewModelContext(const UClass* InClass, FName InViewModelName)
{
	if (InClass && InClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
	{
		ViewModelContextId = FGuid::NewGuid();
		NotifyFieldValueClass = const_cast<UClass*>(InClass);
		ViewModelName = InViewModelName;
	}
}


FText FMVVMBlueprintViewModelContext::GetDisplayName() const
{
	return FText::FromName(ViewModelName);
}

