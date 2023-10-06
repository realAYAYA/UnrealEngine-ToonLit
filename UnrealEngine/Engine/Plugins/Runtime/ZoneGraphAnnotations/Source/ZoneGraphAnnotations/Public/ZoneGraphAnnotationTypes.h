// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphAnnotationTypes.generated.h"

ZONEGRAPHANNOTATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogZoneGraphAnnotations, Warning, All);

/** Base structs for all events for UZoneGraphAnnotationSubsystem. */
USTRUCT()
struct ZONEGRAPHANNOTATIONS_API FZoneGraphAnnotationEventBase
{
	GENERATED_BODY()
};

class ZONEGRAPHANNOTATIONS_API FMassLaneObstacleID
{
public:
	FMassLaneObstacleID() {}
	
	static const FMassLaneObstacleID InvalidID;
	static FMassLaneObstacleID GetNextUniqueID()
	{
		check(NextUniqueID < MAX_uint64); // Ran out of FMassLaneObstacleID.
		return FMassLaneObstacleID(NextUniqueID++);
	}
	
	bool operator==(const FMassLaneObstacleID& Other) const
	{
		return Value == Other.Value;
	}

	uint64 GetValue() const { return Value; }

	bool IsValid() const { return Value != MAX_uint64; }

private:
	static uint64 NextUniqueID;
	
	FMassLaneObstacleID(uint64 ID) : Value(ID) {}
	uint64 Value = MAX_uint64;
};

FORCEINLINE uint32 GetTypeHash(const FMassLaneObstacleID& Obs)
{
	return ::GetTypeHash(Obs.GetValue());
}
