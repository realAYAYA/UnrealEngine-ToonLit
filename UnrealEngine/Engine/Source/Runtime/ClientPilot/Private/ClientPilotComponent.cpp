// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientPilotComponent.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClientPilotComponent)


UClientPilotComponent::UClientPilotComponent()
{
}

UClientPilotBlackboard* UClientPilotComponent::GetBlackboardInstance()
{
	return UClientPilotBlackboardManager::GetInstance() ? UClientPilotBlackboardManager::GetInstance()->PilotBlackboard : nullptr;
}

void UClientPilotComponent::ThinkAndAct() 
{
}

IMPLEMENT_MODULE(FDefaultModuleImpl, ClientPilot);
