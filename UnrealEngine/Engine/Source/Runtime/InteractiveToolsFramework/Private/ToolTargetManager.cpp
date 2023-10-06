// Copyright Epic Games, Inc. All Rights Reserved.


#include "ToolTargetManager.h"
#include "InteractiveToolsContext.h"
#include "ToolBuilderUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ToolTargetManager)

void UToolTargetManager::Initialize()
{
	bIsActive = true;
}

void UToolTargetManager::Shutdown()
{
	Factories.Empty();
	bIsActive = false;
}

void UToolTargetManager::AddTargetFactory(UToolTargetFactory* Factory)
{
	// If this type of factory has already been added, skip it.
	if (Factories.ContainsByPredicate(
		[Factory](UToolTargetFactory* ExistingFactory) { 
			return ExistingFactory->GetClass() == Factory->GetClass(); 
		}))
	{
		return;
	}

	Factories.Add(Factory);
}


UToolTargetFactory* UToolTargetManager::FindFirstFactoryByPredicate(TFunctionRef<bool(UToolTargetFactory*)> Predicate)
{
	TObjectPtr<UToolTargetFactory>* Found = Factories.FindByPredicate(Predicate);
	return (Found) ? Found->Get() : nullptr;
}


bool UToolTargetManager::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetType) const
{
	for (UToolTargetFactory* Factory : Factories)
	{
		if (Factory->CanBuildTarget(SourceObject, TargetType))
		{
			return true;
		}
	}
	return false;
}

UToolTarget* UToolTargetManager::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetType)
{
	for (UToolTargetFactory* Factory : Factories)
	{
		if (Factory->CanBuildTarget(SourceObject, TargetType))
		{
			UToolTarget* Result = Factory->BuildTarget(SourceObject, TargetType);
			if (Result != nullptr)
			{
				return Result;
			}
		}
	}
	return nullptr;
}

int32 UToolTargetManager::CountSelectedAndTargetable(const FToolBuilderState& SceneState, const FToolTargetTypeRequirements& TargetType) const
{
	return ToolBuilderUtil::CountComponents(SceneState, [&](UActorComponent* Object)
		{
			return CanBuildTarget(Object, TargetType);
		});
}


void UToolTargetManager::EnumerateSelectedAndTargetableComponents(const FToolBuilderState& SceneState,
	const FToolTargetTypeRequirements& TargetRequirements,
	TFunctionRef<void(UActorComponent*)> ComponentFunc) const
{
	ToolBuilderUtil::EnumerateComponents(SceneState, [&](UActorComponent* Component)
	{
		if (CanBuildTarget(Component, TargetRequirements))
		{
			ComponentFunc(Component);
		}
	});
}


int32 UToolTargetManager::CountSelectedAndTargetableWithPredicate(const FToolBuilderState& SceneState,
	const FToolTargetTypeRequirements& TargetRequirements,
	TFunctionRef<bool(UActorComponent&)> ComponentPred) const
{
	return ToolBuilderUtil::CountComponents(SceneState, [&](UActorComponent* Object)
	{
		return CanBuildTarget(Object, TargetRequirements) && ComponentPred(*Object);
	});
}

UToolTarget* UToolTargetManager::BuildFirstSelectedTargetable(const FToolBuilderState& SceneState, const FToolTargetTypeRequirements& TargetType)
{
	return BuildTarget(
		ToolBuilderUtil::FindFirstComponent(SceneState, [&](UActorComponent* Object)
		{
			return CanBuildTarget(Object, TargetType);
		}),
		TargetType);
}

TArray<TObjectPtr<UToolTarget>> UToolTargetManager::BuildAllSelectedTargetable(const FToolBuilderState& SceneState,
	const FToolTargetTypeRequirements& TargetType)
{
	TArray<UActorComponent*> Components = ToolBuilderUtil::FindAllComponents(SceneState, [&](UActorComponent* Object)
	{
		return CanBuildTarget(Object, TargetType);
	});
	TArray<TObjectPtr<UToolTarget>> Targets;
	Targets.Reserve(Components.Num());
	for (UActorComponent* Component : Components)
	{
		Targets.Add(BuildTarget(Component, TargetType));
	}
	return Targets;
}

