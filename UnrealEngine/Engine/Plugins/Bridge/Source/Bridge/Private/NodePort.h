// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "NodePort.generated.h"

UCLASS()
class UNodePort :
	public UObject
{
	GENERATED_UCLASS_BODY()
public:
	FString PortFilePath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Bridge"), TEXT("ThirdParty"), TEXT("node_port.txt"));
	bool bIsNodeRunning;
	UFUNCTION()
		FString GetNodePort();

	UFUNCTION()
		bool IsNodeRunning();
};
