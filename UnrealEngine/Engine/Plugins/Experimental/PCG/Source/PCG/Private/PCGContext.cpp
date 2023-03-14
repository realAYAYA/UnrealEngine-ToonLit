// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGContext.h"
#include "PCGComponent.h"
#include "GameFramework/Actor.h"

FString FPCGContext::GetTaskName() const
{
	if (Node)
	{
		const FName NodeName = ((Node->NodeTitle != NAME_None) ? Node->NodeTitle : Node->GetFName());

		const UPCGSettings* Settings = GetInputSettings<UPCGSettings>();
		const FName NodeAdditionalName = Settings ? Settings->AdditionalTaskName() : NAME_None;

		if (NodeAdditionalName == NAME_None || NodeAdditionalName == NodeName)
		{
			return NodeName.ToString();
		}
		else
		{
			return FString::Printf(TEXT("%s (%s)"), *NodeName.ToString(), *NodeAdditionalName.ToString());
		}
	}
	else
	{
		return TEXT("Anonymous task");
	}
}

FString FPCGContext::GetComponentName() const
{
	return SourceComponent.IsValid() && SourceComponent->GetOwner() ? SourceComponent->GetOwner()->GetFName().ToString() : TEXT("Non-PCG Component");
}

bool FPCGContext::ShouldStop() const
{
	return FPlatformTime::Seconds() > EndTime;
}