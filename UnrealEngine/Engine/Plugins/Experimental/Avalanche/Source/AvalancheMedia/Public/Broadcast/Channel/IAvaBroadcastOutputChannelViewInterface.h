// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAvaBroadcastOutputChannelViewInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UAvaBroadcastOutputChannelViewInterface : public UInterface
{
	GENERATED_BODY()
};

class IAvaBroadcastOutputChannelViewInterface
{
	GENERATED_BODY()
	
public:
	
	UFUNCTION(BlueprintNativeEvent, Category = "Motion Design Output Channel")
	void SetChannelName(const FText& InChannelName);
};
