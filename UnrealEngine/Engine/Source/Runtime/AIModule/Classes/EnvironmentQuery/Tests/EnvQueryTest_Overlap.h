// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_Overlap.generated.h"

class AActor;
struct FCollisionQueryParams;
struct FCollisionShape;

UCLASS(MinimalAPI)
class UEnvQueryTest_Overlap : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

	/** Overlap data */
	UPROPERTY(EditDefaultsOnly, Category=Overlap)
	FEnvOverlapData OverlapData;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:

	bool RunOverlap(const FVector& ItemPos, const FCollisionShape& CollisionShape, const TArray<AActor*>& IgnoredActors, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params) const;
	bool RunOverlapBlocking(const FVector& ItemPos, const FCollisionShape& CollisionShape, const TArray<AActor*>& IgnoredActors, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params) const;

    UE_DEPRECATED(4.23, "This method is no longer called by RunTest and has been replaced by an overload using a list of actors to ignore. It now calls that overload but you should use the new overload instead.")
	bool RunOverlap(const FVector& ItemPos, const FCollisionShape& CollisionShape, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params);

	UE_DEPRECATED(4.23, "This method is no longer called by RunTest and has been replaced by an overload using a list of actors to ignore. It now calls that overload but you should use the new overload instead.")
	bool RunOverlapBlocking(const FVector& ItemPos, const FCollisionShape& CollisionShape, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params);
};
