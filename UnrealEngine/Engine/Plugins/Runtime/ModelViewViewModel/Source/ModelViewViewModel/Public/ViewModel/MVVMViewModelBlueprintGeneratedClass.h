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


/** Will be deprecated in the next version. */
UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMViewModelBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#endif
