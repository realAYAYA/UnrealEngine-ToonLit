// Copyright Epic Games, Inc. All Rights Reserved.
#include "AIResources.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AIResources)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FString UAIResource_Movement::GenerateDebugDescription() const
{
	return TEXT("Move");
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

