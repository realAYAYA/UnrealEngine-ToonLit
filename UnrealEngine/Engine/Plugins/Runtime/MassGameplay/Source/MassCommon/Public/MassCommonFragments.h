// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "AI/Navigation/NavigationTypes.h"
#include "MassCommonFragments.generated.h"


USTRUCT()
struct MASSCOMMON_API FTransformFragment : public FMassFragment
{
	GENERATED_BODY()

	const FTransform& GetTransform() const { return Transform; }
	void SetTransform(const FTransform& InTransform) { Transform = InTransform; }
	FTransform& GetMutableTransform() { return Transform; }

protected:
	UPROPERTY(Transient)
	FTransform Transform;
};

USTRUCT()
struct MASSCOMMON_API FAgentRadiusFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	float Radius = 40.f;
};

/** This is a common type for all the wrappers pointing at UObjects used to copy data from them or set data based on
 *	Mass simulation..
 */
USTRUCT()
struct FObjectWrapperFragment : public FMassFragment
{
	GENERATED_BODY()
};
