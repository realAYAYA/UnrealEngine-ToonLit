// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterProjectionBlueprintLib.h"
#include "Blueprints/DisplayClusterProjectionBlueprintAPIImpl.h"
#include "UObject/Package.h"


UDisplayClusterProjectionBlueprintLib::UDisplayClusterProjectionBlueprintLib(class FObjectInitializer const & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDisplayClusterProjectionBlueprintLib::GetAPI(TScriptInterface<IDisplayClusterProjectionBlueprintAPI>& OutAPI)
{
	static UDisplayClusterProjectionBlueprintAPIImpl* Obj = NewObject<UDisplayClusterProjectionBlueprintAPIImpl>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	OutAPI = Obj;
}
