// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMViewModelBase.h"

#include "Bindings/MVVMBindingHelper.h"
#include "ViewModel/MVVMViewModelBlueprintGeneratedClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewModelBase)

#define LOCTEXT_NAMESPACE "MVVMViewModelBase"

FDelegateHandle UMVVMViewModelBase::AddFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate)
{
	FDelegateHandle Result;
	if (InFieldId.IsValid())
	{
		Result = Delegates.Add(this, InFieldId, MoveTemp(InNewDelegate));
		if (Result.IsValid())
		{
			EnabledFieldNotifications.PadToNum(InFieldId.GetIndex() + 1, false);
			EnabledFieldNotifications[InFieldId.GetIndex()] = true;
		}
	}
	return Result;
}


void UMVVMViewModelBase::K2_AddFieldValueChangedDelegate(FFieldNotificationId InFieldId, FFieldValueChangedDynamicDelegate InDelegate)
{
	if (InFieldId.IsValid())
	{
		const UE::FieldNotification::FFieldId FieldId = GetFieldNotificationDescriptor().GetField(GetClass(), InFieldId.FieldName);
		if (ensureMsgf(FieldId.IsValid(), TEXT("The field should be compiled correctly.")))
		{
			FDelegateHandle Result = Delegates.Add(this, FieldId, InDelegate);
			if (Result.IsValid())
			{
				EnabledFieldNotifications.PadToNum(FieldId.GetIndex() + 1, false);
				EnabledFieldNotifications[FieldId.GetIndex()] = true;
			}
		}
	}
}


bool UMVVMViewModelBase::RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle)
{
	bool bResult = false;
	if (InFieldId.IsValid() && InHandle.IsValid()  && EnabledFieldNotifications.IsValidIndex(InFieldId.GetIndex()) && EnabledFieldNotifications[InFieldId.GetIndex()])
	{
		UE::FieldNotification::FFieldMulticastDelegate::FRemoveFromResult RemoveResult = Delegates.RemoveFrom(this, InFieldId, InHandle);
		bResult = RemoveResult.bRemoved;
		EnabledFieldNotifications[InFieldId.GetIndex()] = RemoveResult.bHasOtherBoundDelegates;
	}
	return bResult;
}


void UMVVMViewModelBase::K2_RemoveFieldValueChangedDelegate(FFieldNotificationId InFieldId, FFieldValueChangedDynamicDelegate InDelegate)
{
	if (InFieldId.IsValid())
	{
		const UE::FieldNotification::FFieldId FieldId = GetFieldNotificationDescriptor().GetField(GetClass(), InFieldId.FieldName);
		if (ensureMsgf(FieldId.IsValid(), TEXT("The field should be compiled correctly.")))
		{
			if (EnabledFieldNotifications.IsValidIndex(FieldId.GetIndex()) && EnabledFieldNotifications[FieldId.GetIndex()])
			{
				UE::FieldNotification::FFieldMulticastDelegate::FRemoveFromResult RemoveResult = Delegates.RemoveFrom(this, FieldId, InDelegate);
				EnabledFieldNotifications[FieldId.GetIndex()] = RemoveResult.bHasOtherBoundDelegates;
			}
		}
	}
}


int32 UMVVMViewModelBase::RemoveAllFieldValueChangedDelegates(const void* InUserObject)
{
	int32 bResult = 0;
	if (InUserObject)
	{
		UE::FieldNotification::FFieldMulticastDelegate::FRemoveAllResult RemoveResult = Delegates.RemoveAll(this, InUserObject);
		bResult = RemoveResult.RemoveCount;
		EnabledFieldNotifications = RemoveResult.HasFields;
	}
	return bResult;
}


int32 UMVVMViewModelBase::RemoveAllFieldValueChangedDelegates(UE::FieldNotification::FFieldId InFieldId, const void* InUserObject)
{
	int32 bResult = 0;
	if (InUserObject)
	{
		UE::FieldNotification::FFieldMulticastDelegate::FRemoveAllResult RemoveResult = Delegates.RemoveAll(this, InFieldId, InUserObject);
		bResult = RemoveResult.RemoveCount;
		EnabledFieldNotifications = RemoveResult.HasFields;
	}
	return bResult;
}


const UE::FieldNotification::IClassDescriptor& UMVVMViewModelBase::GetFieldNotificationDescriptor() const
{
	static FFieldNotificationClassDescriptor Local;
	return Local;
}


void UMVVMViewModelBase::FFieldNotificationClassDescriptor::ForEachField(const UClass* Class, TFunctionRef<bool(::UE::FieldNotification::FFieldId FieldId)> Callback) const
{
	if (const UMVVMViewModelBlueprintGeneratedClass* ViewModelBPClass = Cast<const UMVVMViewModelBlueprintGeneratedClass>(Class))
	{
		ViewModelBPClass->ForEachField(Callback);
	}
}


void UMVVMViewModelBase::MarkRepDirty(int32 InRepIndex)
{
#if WITH_PUSH_MODEL
	check(InRepIndex >= 0 && InRepIndex < GetClass()->ClassReps.Num());
	if (IS_PUSH_MODEL_ENABLED())
	{
		MARK_PROPERTY_DIRTY_UNSAFE(this, InRepIndex);
	}
#endif
}


void UMVVMViewModelBase::MarkRepDirty(FProperty* InProperty)
{
#if WITH_PUSH_MODEL
	check(InProperty);
	if (IS_PUSH_MODEL_ENABLED() && InProperty->RepIndex >= 0)
	{
		MARK_PROPERTY_DIRTY(this, InProperty);
	}
#endif
}


void UMVVMViewModelBase::BroadcastFieldValueChanged(UE::FieldNotification::FFieldId InFieldId)
{
	if (InFieldId.IsValid() && EnabledFieldNotifications.IsValidIndex(InFieldId.GetIndex()) && EnabledFieldNotifications[InFieldId.GetIndex()])
	{
		Delegates.Broadcast(this, InFieldId);
	}
}


void UMVVMViewModelBase::K2_BroadcastFieldValueChanged(FFieldNotificationId ViewModelPropertyName)
{
	UE::FieldNotification::FFieldId FieldId = GetFieldNotificationDescriptor().GetField(GetClass(), ViewModelPropertyName.GetFieldName());
	BroadcastFieldValueChanged(FieldId);
}


DEFINE_FUNCTION(UMVVMViewModelBase::execK2_SetPropertyValue)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	FProperty* TargetProperty = Stack.MostRecentProperty;
	void* TargetValuePtr = Stack.MostRecentPropertyAddress;

	Stack.StepCompiledIn<FProperty>(nullptr);
	FProperty* SourceProperty = Stack.MostRecentProperty;
	void* SourceValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	UMVVMViewModelBase* ViewModelContext = Cast<UMVVMViewModelBase>(Context);

	bool bResult = false;
	if (ViewModelContext == nullptr || TargetProperty == nullptr || SourceProperty == nullptr)
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AccessViolation,
			LOCTEXT("MissingInputProperty", "Failed to resolve the input parameter for SetPropertyValue.")
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN

		UE::FieldNotification::FFieldId FieldId = ViewModelContext->GetFieldNotificationDescriptor().GetField(ViewModelContext->GetClass(), TargetProperty->GetFName());
		if (!FieldId.IsValid())
		{
			const FBlueprintExceptionInfo ExceptionInfo(
				EBlueprintExceptionType::AccessViolation,
				LOCTEXT("Bad FieldId", "Failed to find the FieldId that correspond to the set value.")
			);
			FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		}
		else
		{
			bResult = TargetProperty->Identical(TargetValuePtr, SourceValuePtr);
			if (!bResult)
			{
				// Set the value then notify that the value changed.
				TargetProperty->SetValue_InContainer(ViewModelContext, SourceValuePtr);
				ViewModelContext->MarkRepDirty(TargetProperty);
				ViewModelContext->BroadcastFieldValueChanged(FieldId);
			}
		}

		P_NATIVE_END
	}

	*StaticCast<bool*>(RESULT_PARAM) = (!bResult);
}

#undef LOCTEXT_NAMESPACE

