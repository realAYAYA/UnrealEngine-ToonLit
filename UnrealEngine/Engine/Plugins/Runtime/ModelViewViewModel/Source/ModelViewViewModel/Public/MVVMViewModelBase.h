// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "FieldNotification/FieldId.h"
#include "FieldNotification/FieldNotificationDeclaration.h"
#include "FieldNotification/FieldMulticastDelegate.h"
#include "FieldNotification/IFieldValueChanged.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Types/MVVMAvailableBinding.h"
#include "Types/MVVMBindingName.h"

#include "MVVMViewModelBase.generated.h"

/** After a field value changed. Broadcast the event (doesn't execute the replication code). */
#define UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MemberName) \
	BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::MemberName)

/** After a field value changed. Replicate and broadcast the event. */
#define UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED_WITH_REP(MemberName ) \
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, this, MemberName); \
	BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::MemberName)

/** If the property value changed then set the new value and notify. */
#define UE_MVVM_SET_PROPERTY_VALUE(MemberName, NewValue) \
	SetPropertyValue(MemberName, NewValue, ThisClass::FFieldNotificationClassDescriptor::MemberName)
	
/** If the property value changed then set the new value, replicate and notify. */
#define UE_MVVM_SET_PROPERTY_VALUE_WITH_REP(MemberName, NewValue) \
	SetPropertyValue(MemberName, NewValue, ThisClass::FFieldNotificationClassDescriptor::MemberName, ThisClass::ENetFields_Private::MemberName)


/** Base class for MVVM viewmodel. */
UCLASS(NotBlueprintable, Abstract, DisplayName="MVVM ViewModel")
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
	//~ End INotifyFieldValueChanged Interface

	UFUNCTION(BlueprintCallable, Category = "FieldNotify", meta = (DisplayName = "Add Field Value Changed Delegate", ScriptName = "AddFieldValueChangedDelegate"))
	void K2_AddFieldValueChangedDelegate(FFieldNotificationId FieldId, FFieldValueChangedDynamicDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category = "FieldNotify", meta = (DisplayName = "Remove Field Value Changed Delegate", ScriptName = "RemoveFieldValueChangedDelegate"))
	void K2_RemoveFieldValueChangedDelegate(FFieldNotificationId FieldId, FFieldValueChangedDynamicDelegate Delegate);

protected:
	/** Execute the replication code. */
	void MarkRepDirty(FProperty* InProperty);
private:
	void MarkRepDirty(int32 InRepIndex);

protected:
	UFUNCTION(BlueprintCallable, Category="FieldNotify", meta=(DisplayName="Broadcast Field Value Changed", ScriptName="BroadcastFieldValueChanged", BlueprintInternalUseOnly="true"))
	void K2_BroadcastFieldValueChanged(FFieldNotificationId FieldId);

	/** Broadcast the event (doesn't execute the replication code). */
	void BroadcastFieldValueChanged(UE::FieldNotification::FFieldId InFieldId);

protected:
	UFUNCTION(BlueprintCallable, CustomThunk, Category="Viewmodel", meta=(CustomStructureParam="OldValue,NewValue", ScriptName="SetPropertyValue", BlueprintInternalUseOnly="true"))
	bool K2_SetPropertyValue(UPARAM(ref) const int32& OldValue, UPARAM(ref) const int32& NewValue);

	/** Set the new value and notify if the property value changed. */
	template<typename T, typename U>
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

	/** Set the new value and notify if the property value changed. */
	template<typename T, typename U, typename NetFieldsEnum>
	bool SetPropertyValue(T& Value, const U& NewValue, UE::FieldNotification::FFieldId FieldId, NetFieldsEnum InRepIndex)
	{
		if (Value == NewValue)
		{
			return false;
		}

		Value = NewValue;
		MarkRepDirty((int32)InRepIndex);
		BroadcastFieldValueChanged(FieldId);
		return true;
	}

private:
	DECLARE_FUNCTION(execK2_SetPropertyValue);

private:
	UE::FieldNotification::FFieldMulticastDelegate Delegates;
	TBitArray<> EnabledFieldNotifications;
};
