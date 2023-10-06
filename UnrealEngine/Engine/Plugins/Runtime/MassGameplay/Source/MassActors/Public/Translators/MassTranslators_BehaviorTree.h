// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassTranslator.h"
#include "MassCommonFragments.h"
#include "MassTranslators_BehaviorTree.generated.h"

//////////////////////////////////////////////////////////////////////////
class UBehaviorTreeComponent;

USTRUCT()
struct FDataFragment_BehaviorTreeComponentWrapper : public FObjectWrapperFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<UBehaviorTreeComponent> Component;
};

UCLASS()
class MASSACTORS_API UMassTranslator_BehaviorTree : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassTranslator_BehaviorTree();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override {}

	FMassEntityQuery EntityQuery;
};
