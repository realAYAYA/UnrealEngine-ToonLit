// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#include "FieldNotification/IClassDescriptor.h"
#include "UObject/Interface.h"

#include "IFieldValueChanged.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FFieldValueChangedDynamicDelegate, UObject*, Object, FFieldNotificationId, Field);


UINTERFACE(MinimalAPI, NotBlueprintable)
class UNotifyFieldValueChanged : public UInterface
{
	GENERATED_BODY()
};


class INotifyFieldValueChanged : public IInterface
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FFieldValueChangedMulticastDelegate, UObject*, UE::FieldNotification::FFieldId);
	using FFieldValueChangedDelegate = FFieldValueChangedMulticastDelegate::FDelegate;

public:
	/** Add a delegate that will be notified when the FieldId is value changed. */
	virtual FDelegateHandle AddFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate) = 0;

	/** Remove a delegate that was added. */
	virtual bool RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle) = 0;

	/** Remove all the delegate that are bound to the specified UserObject. */
	virtual int32 RemoveAllFieldValueChangedDelegates(const void* InUserObject) = 0;

	/** Remove all the delegate that are bound to the specified Field and UserObject. */
	virtual int32 RemoveAllFieldValueChangedDelegates(UE::FieldNotification::FFieldId InFieldId, const void* InUserObject) = 0;

	/** @returns the list of all the field that can notify when their value changes. */
	virtual const UE::FieldNotification::IClassDescriptor& GetFieldNotificationDescriptor() const = 0;
};
