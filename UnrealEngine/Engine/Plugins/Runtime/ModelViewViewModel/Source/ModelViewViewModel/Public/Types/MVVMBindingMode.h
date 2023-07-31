// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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


/** */
UENUM()
enum class EMVVMViewBindingUpdateMode : uint8
{
	/** Execute the binding as soon as the source value changes. */
	Immediate,
	///** Execute the binding at the end of the frame before drawing when the source value changes. */
	//Delayed,
	///** Always execute the binding at the end of the frame before drawing. */
	//Debug,
};


namespace UE::MVVM
{
	UE_NODISCARD MODELVIEWVIEWMODEL_API bool IsForwardBinding(EMVVMBindingMode Mode);
	UE_NODISCARD MODELVIEWVIEWMODEL_API bool IsBackwardBinding(EMVVMBindingMode Mode);
	UE_NODISCARD MODELVIEWVIEWMODEL_API bool IsOneTimeBinding(EMVVMBindingMode Mode);
}