// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassCommonTypes.h"
#include "MassMovementTypes.h"
#include "RandomSequence.h"
#include "MassMovementFragments.generated.h"

USTRUCT()
struct MASSMOVEMENT_API FMassVelocityFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Value = FVector::ZeroVector;
};

USTRUCT()
struct MASSMOVEMENT_API FMassForceFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Value = FVector::ZeroVector;
};

USTRUCT()
struct MASSMOVEMENT_API FMassMovementParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	FMassMovementParameters GetValidated() const
	{
		FMassMovementParameters Copy = *this;
		Copy.Update();
		return Copy;
	}

	/** Updates internal values for faster desired speed generation. */
	void Update();

	/** Generates desired speed based on style and unique id. The id is used deterministically assign a specific speed range. */
	float GenerateDesiredSpeed(const FMassMovementStyleRef& Style, const int32 UniqueId) const
	{
		float DesiredSpeed = DefaultDesiredSpeed;
		float DesiredSpeedVariance = DefaultDesiredSpeedVariance;
		
		const FMassMovementStyleParameters* StyleParams = MovementStyles.FindByPredicate([&Style](const FMassMovementStyleParameters& Config)
			{
				return Config.Style.ID == Style.ID;
			});
		
		if (StyleParams != nullptr)
		{
			const float Prob = UE::RandomSequence::FRand(UniqueId);
			for (const FMassMovementStyleSpeedParameters& Speed : StyleParams->DesiredSpeeds)
			{
				if (Prob < Speed.ProbabilityThreshold)
				{
					DesiredSpeed = Speed.Speed;
					DesiredSpeedVariance = Speed.Variance;
					break;
				}
			}
		}
		
		return DesiredSpeed * FMath::RandRange(1.0f - DesiredSpeedVariance, 1.0f + DesiredSpeedVariance);
	}
	
	/** Maximum speed (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm/s"))
	float MaxSpeed = 200.f;

	/** 200..600 Smaller steering maximum acceleration makes the steering more \"calm\" but less opportunistic, may not find solution, or gets stuck. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (UIMin = 0.0, ClampMin = 0.0, ForceUnits="cm/s^2"))
	float MaxAcceleration = 250.f;

	/** Default desired speed (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm/s"))
	float DefaultDesiredSpeed = 140.f;

	/** How much default desired speed is varied randomly. */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float DefaultDesiredSpeedVariance = 0.1f;

	/** The time it takes the entity position to catchup to the requested height. */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ForceUnits="s"))
	float HeightSmoothingTime = 0.2f;

	/** List of supported movement styles for this configuration. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	TArray<FMassMovementStyleParameters> MovementStyles;
};
