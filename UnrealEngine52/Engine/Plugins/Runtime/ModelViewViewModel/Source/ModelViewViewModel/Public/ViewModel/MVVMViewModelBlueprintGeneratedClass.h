// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/BlueprintGeneratedClass.h"

#include "MVVMViewModelBlueprintGeneratedClass.generated.h"

namespace UE::FieldNotification { struct FFieldId; }
struct FFieldNotificationId;

class UMVVMViewModelBase;

namespace UE::MVVM
{
	class FViewModelBlueprintCompilerContext;
}//namespace

UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMViewModelBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

	friend UE::MVVM::FViewModelBlueprintCompilerContext;

public:
	UMVVMViewModelBlueprintGeneratedClass();

	//~ Begin UBlueprintGeneratedClass interface
	virtual void PostLoadDefaultObject(UObject* Object) override;
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	//~ End UBlueprintGeneratedClass interface

	void InitializeFieldNotification(const UMVVMViewModelBase* ViewModel);
	void ForEachField(TFunctionRef<bool(::UE::FieldNotification::FFieldId FieldId)> Callback) const;

private:
	/** List Field Notifies. No index here on purpose to prevent saving them. */
	UPROPERTY()
	TArray<FFieldNotificationId> FieldNotifyNames;

	int32 FieldNotifyStartBitNumber;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#endif
