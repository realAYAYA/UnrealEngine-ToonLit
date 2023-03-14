// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_InAppPurchaseQueryOwned2.h"
#include "InAppPurchaseCallbackProxy2.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(K2Node_InAppPurchaseQueryOwned2)

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_InAppPurchaseQueryOwned2::UK2Node_InAppPurchaseQueryOwned2(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UInAppPurchaseCallbackProxy2, CreateProxyObjectForInAppPurchaseQueryOwned);
	ProxyFactoryClass = UInAppPurchaseCallbackProxy2::StaticClass();

	ProxyClass = UInAppPurchaseCallbackProxy2::StaticClass();
}

#undef LOCTEXT_NAMESPACE

