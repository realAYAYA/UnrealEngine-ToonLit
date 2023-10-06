// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassCommonFragments.h"
#include "MassTranslator.h"
#include "MassObserverProcessor.h"
#include "MassCharacterMovementTranslators.generated.h"

class UCharacterMovementComponent;

USTRUCT()
struct FCharacterMovementComponentWrapperFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<UCharacterMovementComponent> Component;
};

USTRUCT()
struct FMassCharacterMovementCopyToMassTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class MASSACTORS_API UMassCharacterMovementToMassTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassCharacterMovementToMassTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

USTRUCT()
struct FMassCharacterMovementCopyToActorTag : public FMassTag
{
	GENERATED_BODY()
};


UCLASS()
class MASSACTORS_API UMassCharacterMovementToActorTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassCharacterMovementToActorTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};


USTRUCT()
struct FMassCharacterOrientationCopyToMassTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class MASSACTORS_API UMassCharacterOrientationToMassTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassCharacterOrientationToMassTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

USTRUCT()
struct FMassCharacterOrientationCopyToActorTag : public FMassTag
{
	GENERATED_BODY()
};


UCLASS()
class MASSACTORS_API UMassCharacterOrientationToActorTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassCharacterOrientationToActorTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
