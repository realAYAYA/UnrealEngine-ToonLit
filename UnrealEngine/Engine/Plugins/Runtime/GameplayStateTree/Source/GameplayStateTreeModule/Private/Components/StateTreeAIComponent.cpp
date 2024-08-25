// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeAIComponent.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "StateTreeExecutionContext.h"

TSubclassOf<UStateTreeSchema> UStateTreeAIComponent::GetSchema() const
{
	return UStateTreeAIComponentSchema::StaticClass();
}

bool UStateTreeAIComponent::SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors /*= false*/)
{
	Context.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateUObject(this, &UStateTreeAIComponent::CollectExternalData));
	return UStateTreeAIComponentSchema::SetContextRequirements(*this, Context, bLogErrors);
}
