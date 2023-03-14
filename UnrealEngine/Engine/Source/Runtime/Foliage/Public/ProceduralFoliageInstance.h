// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "ProceduralFoliageInstance.generated.h"

class UActorComponent;
class UFoliageType;
struct FProceduralFoliageInstance;

UENUM(BlueprintType)
namespace ESimulationOverlap
{
	enum Type
	{
		/*Instances overlap with collision*/
		CollisionOverlap,
		/*Instances overlap with shade*/
		ShadeOverlap,
		/*No overlap*/
		None
	};
}

UENUM(BlueprintType)
namespace ESimulationQuery
{
	enum Type
	{
		None = 0 UMETA(Hidden),
		/*Instances overlap with collision*/
		CollisionOverlap = 1,
		/*Instances overlap with shade*/
		ShadeOverlap = 2,
		/*any overlap*/
		AnyOverlap = 3
	};
}

struct FProceduralFoliageInstance;
struct FProceduralFoliageOverlap
{
	FProceduralFoliageOverlap(FProceduralFoliageInstance* InA, FProceduralFoliageInstance* InB, ESimulationOverlap::Type InType)
	: A(InA)
	, B(InB)
	, OverlapType(InType)
	{}

	FProceduralFoliageInstance* A;
	FProceduralFoliageInstance* B;
	ESimulationOverlap::Type OverlapType;
};

USTRUCT(BlueprintType)
struct FOLIAGE_API FProceduralFoliageInstance
{
public:
	GENERATED_USTRUCT_BODY()
	FProceduralFoliageInstance();
	FProceduralFoliageInstance(const FProceduralFoliageInstance& Other);

	static FProceduralFoliageInstance* Domination(FProceduralFoliageInstance* A, FProceduralFoliageInstance* B, ESimulationOverlap::Type OverlapType);

	float GetMaxRadius() const;
	float GetShadeRadius() const;
	float GetCollisionRadius() const;

	bool IsAlive() const { return bAlive; }

	void TerminateInstance();

public:
	UPROPERTY()
	FQuat Rotation;

	UPROPERTY(Category = ProceduralFoliageInstance, EditAnywhere, BlueprintReadWrite)
	FVector Location;

	UPROPERTY(Category = ProceduralFoliageInstance, EditAnywhere, BlueprintReadWrite)
	float Age;

	UPROPERTY(Category = ProceduralFoliageInstance, EditAnywhere, BlueprintReadWrite)
	FVector Normal;

	UPROPERTY()
	float Scale;

	UPROPERTY()
	TObjectPtr<const UFoliageType> Type;

	UActorComponent* BaseComponent;

	bool bBlocker;
private:
	bool bAlive;
};
