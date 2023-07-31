// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassSimpleMovementTrait.generated.h"


USTRUCT()
struct FMassSimpleMovementTag : public FMassTag
{
	GENERATED_BODY()
};


UCLASS(meta = (DisplayName = "Simple Movement"))
class MASSMOVEMENT_API UMassSimpleMovementTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};


UCLASS()
class MASSMOVEMENT_API UMassSimpleMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassSimpleMovementProcessor();
		
protected:	
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
