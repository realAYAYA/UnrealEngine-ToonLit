// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Modules/ModuleManager.h"

class FPixelStreamingServersModule : public IModuleInterface
{
public:
	virtual ~FPixelStreamingServersModule() = default;
	virtual void StartupModule() override;
	int NextPort();

public:
	static FPixelStreamingServersModule& Get();

private:
	FThreadSafeCounter NextGeneratedPort = 0;

};