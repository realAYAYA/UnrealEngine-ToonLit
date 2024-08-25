// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeComponentSchema.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTaskBase.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tasks/StateTreeAITask.h"
#include "VisualLogger/VisualLogger.h"

UStateTreeComponentSchema::UStateTreeComponentSchema()
	: ContextActorClass(AActor::StaticClass())
	, ContextDataDescs({{ FName("Actor"), AActor::StaticClass(), FGuid(0x1D971B00, 0x28884FDE, 0xB5436802, 0x36984FD5) }})
{
}

bool UStateTreeComponentSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
	|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
	|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct());
}

bool UStateTreeComponentSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UStateTreeComponentSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(AActor::StaticClass())
			|| InStruct.IsChildOf(UActorComponent::StaticClass())
			|| InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}

TConstArrayView<FStateTreeExternalDataDesc> UStateTreeComponentSchema::GetContextDataDescs() const
{
	return ContextDataDescs;
}

void UStateTreeComponentSchema::PostLoad()
{
	Super::PostLoad();
	GetContextActorDataDesc().Struct = ContextActorClass.Get();
}

#if WITH_EDITOR
void UStateTreeComponentSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;

	if (Property)
	{
		if (Property->GetOwnerClass() == UStateTreeComponentSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UStateTreeComponentSchema, ContextActorClass))
		{
			GetContextActorDataDesc().Struct = ContextActorClass.Get();
		}
	}
}
#endif

bool UStateTreeComponentSchema::SetContextRequirements(UBrainComponent& BrainComponent, FStateTreeExecutionContext& Context, bool bLogErrors /*= false*/)
{
	if (!Context.IsValid())
	{
		return false;
	}

	// Make sure the actor matches one required.
	AActor* ContextActor = nullptr;
	const UStateTreeComponentSchema* Schema = Cast<UStateTreeComponentSchema>(Context.GetStateTree()->GetSchema());
	if (Schema)
	{
		AAIController* AIOwner = BrainComponent.GetAIOwner();
		if (AAIController* OwnerController = (AIOwner != nullptr) ? AIOwner : Cast<AAIController>(BrainComponent.GetOwner()))
		{
			if (OwnerController && OwnerController->IsA(Schema->GetContextActorClass()))
			{
				ContextActor = OwnerController;
			}
		}
		if (ContextActor == nullptr)
		{
			if (AActor* OwnerActor = (AIOwner != nullptr) ? AIOwner->GetPawn() : BrainComponent.GetOwner())
			{
				if (OwnerActor && OwnerActor->IsA(Schema->GetContextActorClass()))
				{
					ContextActor = OwnerActor;
				}
			}
		}
		if (ContextActor == nullptr && bLogErrors)
		{
			UE_VLOG(BrainComponent.GetOwner(), LogStateTree, Error, TEXT("%s: Could not find context actor of type %s. StateTree will not update."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Schema->GetContextActorClass()));
		}
	}
	else if (bLogErrors)
	{
		UE_VLOG(BrainComponent.GetOwner(), LogStateTree, Error, TEXT("%s: Expected StateTree asset to contain StateTreeComponentSchema. StateTree will not update."), ANSI_TO_TCHAR(__FUNCTION__));
	}

	const FName ActorName(TEXT("Actor"));
	Context.SetContextDataByName(ActorName, FStateTreeDataView(ContextActor));

	bool bResult = Context.AreContextDataViewsValid();
	if (!bResult && bLogErrors)
	{
		UE_VLOG(BrainComponent.GetOwner(), LogStateTree, Error, TEXT("%s: Missing external data requirements. StateTree will not update."), ANSI_TO_TCHAR(__FUNCTION__));
	}

	return bResult;
}

bool UStateTreeComponentSchema::CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, TArrayView<FStateTreeDataView> OutDataViews)
{
	const UWorld* World = Context.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	check(ExternalDataDescs.Num() == OutDataViews.Num());

	AActor* Owner = Cast<AActor>(Context.GetOwner());
	if (!Owner)
	{
		return false;
	}

	AAIController* AIOwner = Cast<AAIController>(Owner);
	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
	{
		const FStateTreeExternalDataDesc& ItemDesc = ExternalDataDescs[Index];
		if (ItemDesc.Struct != nullptr)
		{
			if (ItemDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(ItemDesc.Struct.Get())));
				OutDataViews[Index] = FStateTreeDataView(Subsystem);
			}
			else if (ItemDesc.Struct->IsChildOf(UActorComponent::StaticClass()))
			{
				UActorComponent* Component = Owner->FindComponentByClass(Cast<UClass>(const_cast<UStruct*>(ItemDesc.Struct.Get())));
				OutDataViews[Index] = FStateTreeDataView(Component);
			}
			else if (ItemDesc.Struct->IsChildOf(APawn::StaticClass()))
			{
				APawn* OwnerPawn = (AIOwner != nullptr) ? AIOwner->GetPawn() : Cast<APawn>(Owner);
				OutDataViews[Index] = FStateTreeDataView(OwnerPawn);
			}
			else if (ItemDesc.Struct->IsChildOf(AAIController::StaticClass()))
			{
				AAIController* OwnerController = AIOwner;
				OutDataViews[Index] = FStateTreeDataView(OwnerController);
			}
			else if (ItemDesc.Struct->IsChildOf(AActor::StaticClass()))
			{
				AActor* OwnerActor = (AIOwner != nullptr) ? AIOwner->GetPawn() : Owner;
				OutDataViews[Index] = FStateTreeDataView(OwnerActor);
			}
		}
	}

	return true;
}