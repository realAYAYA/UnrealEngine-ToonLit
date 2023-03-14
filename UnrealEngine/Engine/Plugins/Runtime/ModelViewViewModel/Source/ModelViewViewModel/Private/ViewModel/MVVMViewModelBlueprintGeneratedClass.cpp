// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModel/MVVMViewModelBlueprintGeneratedClass.h"
#include "MVVMViewModelBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewModelBlueprintGeneratedClass)

UMVVMViewModelBlueprintGeneratedClass::UMVVMViewModelBlueprintGeneratedClass()
	: FieldNotifyStartBitNumber(INDEX_NONE)
{}


void UMVVMViewModelBlueprintGeneratedClass::PostLoadDefaultObject(UObject* Object)
{
	Super::PostLoadDefaultObject(Object);
	InitializeFieldNotification(Cast<UMVVMViewModelBase>(Object));
}


void UMVVMViewModelBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);
	FieldNotifyNames.Empty();
}


void UMVVMViewModelBlueprintGeneratedClass::InitializeFieldNotification(const UMVVMViewModelBase* ViewModel)
{
	FieldNotifyStartBitNumber = 0;
	if (ViewModel && FieldNotifyNames.Num())
	{
		int32 NumberOfField = 0;
		ViewModel->GetFieldNotificationDescriptor().ForEachField(this, [&NumberOfField](::UE::FieldNotification::FFieldId FielId)
			{
				++NumberOfField;
				return true;
			});
		FieldNotifyStartBitNumber = NumberOfField - FieldNotifyNames.Num();
		ensureMsgf(FieldNotifyStartBitNumber >= 0, TEXT("The FieldNotifyStartIndex is negative. The number of field should be positive."));
	}
}


void UMVVMViewModelBlueprintGeneratedClass::ForEachField(TFunctionRef<bool(::UE::FieldNotification::FFieldId FieldId)> Callback) const
{
	ensureMsgf(FieldNotifyStartBitNumber >= 0, TEXT("The FieldNotifyStartIndex is negative. The number of field should be positive."));
	for (int32 Index = 0; Index < FieldNotifyNames.Num(); ++Index)
	{
		if (!Callback(UE::FieldNotification::FFieldId(FieldNotifyNames[Index].GetFieldName(), Index + FieldNotifyStartBitNumber)))
		{
			break;
		}
	}
}

