// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MVVMConversionLibrary.h"

ESlateVisibility UMVVMConversionLibrary::Conv_BoolToSlateVisibility(bool bIsVisible, ESlateVisibility TrueCaseVisibility, ESlateVisibility FalseCaseVisibility)
{
	return bIsVisible ? TrueCaseVisibility : FalseCaseVisibility;
}
