// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassCommonFragments.h"
#include "MassTranslator.h"
#include "MassSceneComponentLocationTranslator.generated.h"


USTRUCT()
struct FMassSceneComponentWrapperFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<USceneComponent> Component;
};

USTRUCT()
struct FMassSceneComponentLocationCopyToMassTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class MASSACTORS_API UMassSceneComponentLocationToMassTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassSceneComponentLocationToMassTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};


USTRUCT()
struct FMassSceneComponentLocationCopyToActorTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class MASSACTORS_API UMassSceneComponentLocationToActorTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassSceneComponentLocationToActorTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};