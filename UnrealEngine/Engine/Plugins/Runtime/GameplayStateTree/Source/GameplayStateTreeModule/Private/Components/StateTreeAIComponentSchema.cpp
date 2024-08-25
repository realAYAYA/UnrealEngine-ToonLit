// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeAIComponentSchema.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "StateTreeExecutionContext.h"
#include "Tasks/StateTreeAITask.h"

UStateTreeAIComponentSchema::UStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: AIControllerClass(AAIController::StaticClass())
{
	ContextDataDescs.Emplace(TEXT("AIController"), AIControllerClass.Get(), FGuid(0xEDB3CD97, 0x95F94E0A, 0xBD15207B, 0x98645CDC));
}

void UStateTreeAIComponentSchema::PostLoad()
{
	Super::PostLoad();
	ContextDataDescs[1].Struct = AIControllerClass.Get();
}

bool UStateTreeAIComponentSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return Super::IsStructAllowed(InScriptStruct) || InScriptStruct->IsChildOf(FStateTreeAITaskBase::StaticStruct());
}

bool UStateTreeAIComponentSchema::SetContextRequirements(UBrainComponent& BrainComponent, FStateTreeExecutionContext& Context, bool bLogErrors /*= false*/)
{
	if(!Context.IsValid())
	{
		return false;
	}

	const FName AIControllerName(TEXT("AIController"));
	Context.SetContextDataByName(AIControllerName, FStateTreeDataView(BrainComponent.GetAIOwner()));

	return Super::SetContextRequirements(BrainComponent, Context, bLogErrors);
}

#if WITH_EDITOR
void UStateTreeAIComponentSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (FProperty* Property = PropertyChangedEvent.Property)
	{
		if (Property->GetOwnerClass() == UStateTreeAIComponentSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UStateTreeAIComponentSchema, AIControllerClass))
		{
			ContextDataDescs[1].Struct = AIControllerClass.Get();
		}
	}
}
#endif // WITH_EDITOR
