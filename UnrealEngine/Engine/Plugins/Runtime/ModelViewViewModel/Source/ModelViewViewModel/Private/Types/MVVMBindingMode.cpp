// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/MVVMBindingMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBindingMode)


namespace UE::MVVM
{

	bool IsForwardBinding(EMVVMBindingMode Mode)
	{
		return static_cast<uint8>(Mode) <= static_cast<uint8>(EMVVMBindingMode::TwoWay);
	}

	bool IsBackwardBinding(EMVVMBindingMode Mode)
	{
		return static_cast<uint8>(Mode) >= static_cast<uint8>(EMVVMBindingMode::TwoWay);
	}

	bool IsOneTimeBinding(EMVVMBindingMode Mode)
	{
		return Mode == EMVVMBindingMode::OneTimeToDestination || Mode == EMVVMBindingMode::OneTimeToSource;
	}

} //namespace

