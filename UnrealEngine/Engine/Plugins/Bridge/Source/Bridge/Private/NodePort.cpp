// Copyright Epic Games, Inc. All Rights Reserved.
#include "NodePort.h"
#include "UI/BridgeUIManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NodePort)


UNodePort::UNodePort(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UNodePort::GetNodePort()
{
	FString NodePort;
	if (FFileHelper::LoadFileToString(NodePort, *PortFilePath))
	{
		return NodePort;
	}

	return TEXT("0");
}

bool UNodePort::IsNodeRunning()
{
	return true;
}

