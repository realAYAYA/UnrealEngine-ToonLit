// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPortalRpcResponder;

class FPortalRpcResponderFactory
{
public:
	static TSharedRef<IPortalRpcResponder> Create();
};
