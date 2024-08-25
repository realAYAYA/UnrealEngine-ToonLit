// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "FieldNotificationDeclaration.h" // IWYU pragma: keep
#include "IFieldNotificationClassDescriptor.h"
#include "ViewModel/MVVMFieldNotificationDelegates.h"

#include "MVVMViewModelBase.generated.h"

/** After a field value changed. Broadcast the event. */
#define UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MemberName) \
	BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::MemberName)

/** If the property value changed then set the new value and notify. */
#define UE_MVVM_SET_PROPERTY_VALUE(MemberName, NewValue) \
	SetPropertyValue(MemberName, NewValue, ThisClass::FFieldNotificationClassDescriptor::MemberName)

/** Use this version to set property values that can't be captured as a function arguments (i.e. bitfields). */
#define UE_MVVM_SET_PROPERTY_VALUE_INLINE(MemberName, NewValue) \
	[this, InNewValue = (NewValue)]() { if (MemberName == InNewValue) { return false; } MemberName = InNewValue; BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::MemberName); return true; }()

namespace UMMVVMViewModelBase
{
	// valid keywords for the UCLASS macro
	enum
	{
		// [ClassMetadata] Specifies which ContextCreationType is allowed for that viewmodel. ex: UCLASS(meta=(MVVMAllowedContextCreationType="Manual|CreateInstance"))
		MVVMAllowedContextCreationType,
		// [ClassMetadata] Specifies which ContextCreationType is disallowed for that viewmodel, all other type are allowed. ex: UCLASS(meta=(MVVMDisallowedContextCreationType="Manual|CreateInstance"))
		MVVMDisallowedContextCreationType
	};
}


/** Base class for MVVM viewmodel. */
UCLASS(Blueprintable, Abstract, DisplayName="MVVM Base Viewmodel")
class MODELVIEWVIEWMODEL_API UMVVMViewModelBase : public UObject, public INotifyFieldValueChanged
{
	GENERATED_BODY()

public:
	struct MODELVIEWVIEWMODEL_API FFieldNotificationClassDescriptor : public ::UE::FieldNotification::IClassDescriptor
	{
		virtual void ForEachField(const UClass* Class, TFunctionRef<bool(UE::FieldNotification::FFieldId FielId)> Callback) const override;
	};

public:
	//~ Begin INotifyFieldValueChanged Interface
	virtual FDelegateHandle AddFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate) override final;
	virtual bool RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle) override final;
	virtual int32 RemoveAllFieldValueChangedDelegates(const void* InUserObject) override final;
	virtual int32 RemoveAllFieldValueChangedDelegates(UE::FieldNotification::FFieldId InFieldId, const void* InUserObject) override final;
	virtual const UE::FieldNotification::IClassDescriptor& GetFieldNotificationDescriptor() const override;
	virtual void BroadcastFieldValueChanged(UE::FieldNotification::FFieldId InFieldId) override;
	//~ End INotifyFieldValueChanged Interface

	TArray<UE::FieldNotification::FFieldMulticastDelegate::FDelegateView> GetNotificationDelegateView() const;

	UFUNCTION(BlueprintCallable, Category = "FieldNotify", meta = (DisplayName = "Add Field Value Changed Delegate", ScriptName = "AddFieldValueChangedDelegate"))
	void K2_AddFieldValueChangedDelegate(FFieldNotificationId FieldId, FFieldValueChangedDynamicDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category = "FieldNotify", meta = (DisplayName = "Remove Field Value Changed Delegate", ScriptName = "RemoveFieldValueChangedDelegate"))
	void K2_RemoveFieldValueChangedDelegate(FFieldNotificationId FieldId, FFieldValueChangedDynamicDelegate Delegate);

protected:
	UE_DEPRECATED(5.3, "BroadcastFieldValueChanged has been deprecated, please use the regular blueprint setter.")
	UFUNCTION(BlueprintCallable, Category="FieldNotify", meta=(DisplayName="Broadcast Field Value Changed", ScriptName="BroadcastFieldValueChanged", DeprecatedFunction, DeprecatedMessage = "Use the regular setter node."))
	void K2_BroadcastFieldValueChanged(FFieldNotificationId FieldId);

	UE_DEPRECATED(5.3, "SetPropertyValue has been deprecated, please use the regular blueprint setter.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category="Viewmodel", meta=(CustomStructureParam="OldValue,NewValue", ScriptName="SetPropertyValue", DeprecatedFunction, DeprecatedMessage = "Use the regular setter node.", BlueprintInternalUseOnly="true"))
	bool K2_SetPropertyValue(UPARAM(ref) const int32& OldValue, UPARAM(ref) const int32& NewValue);

	/** Set the new value and notify if the property value changed. */
	template<typename T, typename U = T>
	bool SetPropertyValue(T& Value, const U& NewValue, UE::FieldNotification::FFieldId FieldId)
	{
		if (Value == NewValue)
		{
			return false;
		}

		Value = NewValue;
		BroadcastFieldValueChanged(FieldId);
		return true;
	}

	bool SetPropertyValue(FText& Value, const FText& NewValue, UE::FieldNotification::FFieldId FieldId)
	{
		if (Value.IdenticalTo(NewValue))
		{
			return false;
		}

		Value = NewValue;
		BroadcastFieldValueChanged(FieldId);
		return true;
	}

private:
	DECLARE_FUNCTION(execK2_SetPropertyValue);

private:
	UE::MVVM::FMVVMFieldNotificationDelegates NotificationDelegates;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "FieldNotification/FieldNotificationDeclaration.h"
#include "Types/MVVMAvailableBinding.h"
#include "Types/MVVMBindingName.h"
#endif
