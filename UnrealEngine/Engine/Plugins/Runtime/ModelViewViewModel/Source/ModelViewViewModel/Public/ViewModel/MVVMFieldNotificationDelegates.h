// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FieldNotificationDelegate.h"

class FDelegateHandle;
namespace UE::FieldNotification { struct FFieldId; }

namespace UE::MVVM
{

/** Basic implementation of the INotifyFieldValueChanged implementation. */
class MODELVIEWVIEWMODEL_API FMVVMFieldNotificationDelegates
{
public:
	FDelegateHandle AddFieldValueChangedDelegate(UObject* Owner, UE::FieldNotification::FFieldId InFieldId, INotifyFieldValueChanged::FFieldValueChangedDelegate InNewDelegate);
	void AddFieldValueChangedDelegate(UObject* Owner, UE::FieldNotification::FFieldId FieldId, const FFieldValueChangedDynamicDelegate& Delegate);
	bool RemoveFieldValueChangedDelegate(UObject* Owner, UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle);
	bool RemoveFieldValueChangedDelegate(UObject* Owner, UE::FieldNotification::FFieldId FieldId, const FFieldValueChangedDynamicDelegate& Delegate);
	int32 RemoveAllFieldValueChangedDelegates(UObject* Owner, const void* InUserObject);
	int32 RemoveAllFieldValueChangedDelegates(UObject* Owner, UE::FieldNotification::FFieldId InFieldId, const void* InUserObject);
	void BroadcastFieldValueChanged(UObject* Owner, UE::FieldNotification::FFieldId InFieldId);
	TArray<UE::FieldNotification::FFieldMulticastDelegate::FDelegateView> GetView() const;

private:
	UE::FieldNotification::FFieldMulticastDelegate Delegates;
	TBitArray<> EnabledFieldNotifications;
};

} //namespace

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Net/Core/PushModel/PushModel.h"
#endif
