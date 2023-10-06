// Copyright Epic Games, Inc. All Rights Reserved.

#include "AppleVisionBlueprintSupport.h"
#include "AppleVisionBlueprintProxy.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, AppleVisionBlueprintSupport)

UK2Node_DetectFaces::UK2Node_DetectFaces(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAppleVisionDetectFacesAsyncTaskBlueprintProxy, CreateProxyObjectForDetectFaces);
	ProxyFactoryClass = UAppleVisionDetectFacesAsyncTaskBlueprintProxy::StaticClass();
	ProxyClass = UAppleVisionDetectFacesAsyncTaskBlueprintProxy::StaticClass();
}
