// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBindingMode.generated.h"


/** */
UENUM()
enum class EMVVMBindingMode : uint8
{
	OneTimeToDestination = 0,
	OneWayToDestination,
	TwoWay,
	OneTimeToSource UMETA(Hidden),
	OneWayToSource,
};


namespace UE::MVVM
{
	[[nodiscard]] MODELVIEWVIEWMODEL_API bool IsForwardBinding(EMVVMBindingMode Mode);
	[[nodiscard]] MODELVIEWVIEWMODEL_API bool IsBackwardBinding(EMVVMBindingMode Mode);
	[[nodiscard]] MODELVIEWVIEWMODEL_API bool IsOneTimeBinding(EMVVMBindingMode Mode);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
