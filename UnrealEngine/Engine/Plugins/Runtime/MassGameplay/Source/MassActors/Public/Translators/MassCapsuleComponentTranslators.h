// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassCommonFragments.h"
#include "MassTranslator.h"
#include "MassCapsuleComponentTranslators.generated.h"


class UCapsuleComponent;
struct FAgentRadiusFragment;

USTRUCT()
struct FCapsuleComponentWrapperFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<UCapsuleComponent> Component;
};

/**
 * @todo TBD
 * I'm a bit on a fence regarding having separate tags per copy direction. My concern is that we can end up with a very 
 * dispersed entity population spread across multiple archetypes storing a small number of entities each. An alternative
 * would be to have a property on the Wrapper fragment, but that doesn't sit well with me either, since that data would be 
 * essentially static, meaning it will (in most cases) never change for a given entity, and we could waste a lot of time 
 * iterating over fragments just to check that specific value.
 */
USTRUCT()
struct FMassCapsuleTransformCopyToMassTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class MASSACTORS_API UMassCapsuleTransformToMassTranslator : public UMassTranslator
{
	GENERATED_BODY()
public:
	UMassCapsuleTransformToMassTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;	

	FMassEntityQuery EntityQuery;
};

USTRUCT()
struct FMassCapsuleTransformCopyToActorTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class MASSACTORS_API UMassTransformToActorCapsuleTranslator : public UMassTranslator
{
	GENERATED_BODY()
public:
	UMassTransformToActorCapsuleTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
