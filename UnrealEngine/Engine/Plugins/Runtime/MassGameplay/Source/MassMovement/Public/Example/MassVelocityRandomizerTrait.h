// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassObserverProcessor.h"
#include "MassVelocityRandomizerTrait.generated.h"


class UMassRandomVelocityInitializer;

UCLASS(meta = (DisplayName = "Velocity Randomizer"))
class MASSMOVEMENT_API UMassVelocityRandomizerTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** The speed is expressed in UnrealUnits per second, which usually translates to 0.01m/s */
	UPROPERTY(Category = "Velocity", EditAnywhere, meta = (UIMin = 0.0, ClampMin = 0.0))
	float MinSpeed = 0.f;

	/** The speed is expressed in UnrealUnits per second, which usually translates to 0.01m/s */
	UPROPERTY(Category = "Velocity", EditAnywhere, meta = (UIMin = 1.0, ClampMin = 1.0))
	float MaxSpeed = 200.f;

	UPROPERTY(Category = "Velocity", EditAnywhere)
	bool bSetZComponent = false;
};


UCLASS()
class MASSMOVEMENT_API UMassRandomVelocityInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UMassRandomVelocityInitializer();

	void SetParameters(const float InMinSpeed, const float InMaxSpeed, const bool bInSetZComponent);

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;

	UPROPERTY()
	float MinSpeed = 0.f;

	/** The default max is set to 0 to enforce explicit configuration via SetParameters call. */
	UPROPERTY()
	float MaxSpeed = 0.f;

	UPROPERTY()
	bool bSetZComponent = false;
};
