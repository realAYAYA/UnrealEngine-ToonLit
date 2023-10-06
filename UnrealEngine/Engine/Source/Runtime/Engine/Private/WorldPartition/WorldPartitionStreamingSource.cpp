// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "Misc/HashBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionStreamingSource)

int32 FWorldPartitionStreamingSource::LocationQuantization = 400;
FAutoConsoleVariableRef FWorldPartitionStreamingSource::CVarLocationQuantization(
	TEXT("wp.Runtime.UpdateStreaming.LocationQuantization"),
	FWorldPartitionStreamingSource::LocationQuantization,
	TEXT("Distance (in Unreal units) used to quantize the streaming sources location to determine if a world partition streaming update is necessary."),
	ECVF_Default);

int32 FWorldPartitionStreamingSource::RotationQuantization = 10;
FAutoConsoleVariableRef FWorldPartitionStreamingSource::CVarRotationQuantization(
	TEXT("wp.Runtime.UpdateStreaming.RotationQuantization"),
	FWorldPartitionStreamingSource::RotationQuantization,
	TEXT("Angle (in degrees) used to quantize the streaming sources rotation to determine if a world partition streaming update is necessary."),
	ECVF_Default);

void FWorldPartitionStreamingSource::UpdateHash()
{
	// Update old values when they are changing enough, to avoid the case where we are on the edge of a quantization unit.
	if (!UWorldPartitionStreamingPolicy::IsUpdateStreamingOptimEnabled() || 
		(FVector::Dist(Location, OldLocation) > FWorldPartitionStreamingSource::LocationQuantization))
	{
		OldLocation = Location;
	}
	
	if (!UWorldPartitionStreamingPolicy::IsUpdateStreamingOptimEnabled() ||
		(FMath::Abs(Rotation.Pitch - OldRotation.Pitch) > FWorldPartitionStreamingSource::RotationQuantization) ||
		(FMath::Abs(Rotation.Yaw - OldRotation.Yaw) > FWorldPartitionStreamingSource::RotationQuantization) ||
		(FMath::Abs(Rotation.Roll - OldRotation.Roll) > FWorldPartitionStreamingSource::RotationQuantization))
	{
		OldRotation = Rotation;
	}

	FHashBuilder HashBuilder;
	HashBuilder	<< Name << TargetState << bBlockOnSlowLoading << bReplay << bRemote << Priority << TargetBehavior << TargetGrids << TargetHLODLayers << Shapes;

	if (LocationQuantization)
	{
		HashBuilder << FMath::FloorToInt(OldLocation.X / LocationQuantization) << FMath::FloorToInt(OldLocation.Y / LocationQuantization);
	}

	if (RotationQuantization > 0)
	{
		HashBuilder << FMath::FloorToInt(OldRotation.Yaw / RotationQuantization);
	}

	Hash2D = HashBuilder.GetHash();

	if (LocationQuantization)
	{
		HashBuilder << FMath::FloorToInt(OldLocation.Z / LocationQuantization);
	}

	if (RotationQuantization)
	{
		HashBuilder << FMath::FloorToInt(OldRotation.Pitch / RotationQuantization) << FMath::FloorToInt(OldRotation.Roll / RotationQuantization);
	}

	Hash3D = HashBuilder.GetHash();
};

FORCEINLINE static bool IsClockWise(const FVector2D& V1, const FVector2D& V2)
{
	return (V1 ^ V2) < 0;
}

FORCEINLINE static bool IsVectorInsideVectorPair(const FVector2D& TestVector, const FVector2D& V1, const FVector2D& V2)
{
	return IsClockWise(V1, TestVector) && !IsClockWise(V2, TestVector);
}

FORCEINLINE static bool IsPointInsideSector(const FVector2D& TestPoint, const FVector2D& SectorCenter, float SectorRadiusSquared, const FVector2D& SectorStart, const FVector2D& SectorEnd, float SectorAngle)
{
	const FVector2D TestVector = TestPoint - SectorCenter;
	if (TestVector.SizeSquared() > SectorRadiusSquared)
	{
		return false;
	}

	if (SectorAngle <= 180.0f)
	{
		return IsVectorInsideVectorPair(TestVector, SectorStart, SectorEnd);
	}
	else
	{
		return !IsVectorInsideVectorPair(TestVector, SectorEnd, SectorStart);
	}
}

TArray<TPair<FVector, FVector>> FSphericalSector::BuildDebugMesh() const
{
	TArray<TPair<FVector, FVector>> Segments;
	if (!IsValid())
	{
		return Segments;
	}

	const int32 SegmentCount = FMath::Max(4, FMath::CeilToInt32(64 * (float)Angle / 360.f));
	const FReal AngleStep = Angle / FReal(SegmentCount);
	const FRotator ShapeRotation = FRotationMatrix::MakeFromX(Axis).Rotator();
	const FVector ScaledAxis = FVector::ForwardVector * Radius;
	const int32 RollCount = 16;

	Segments.Reserve(2 * (RollCount + 1) * (SegmentCount + 2));
	int32 LastArcStartIndex = -1;
	for (int32 i = 0; i <= RollCount; ++i)
	{
		const float Roll = 180.f * i / float(RollCount);
		const FTransform Transform(FRotator(0, 0, Roll) + ShapeRotation, Center);
		FVector SegmentStart = Transform.TransformPosition(FRotator(0, -0.5f * Angle, 0).RotateVector(ScaledAxis));
		Segments.Emplace(Center, SegmentStart);
		int32 CurrentArcStartIndex = Segments.Num();
		// Build sector arc
		for (int32 j = 1; j <= SegmentCount; j++)
		{
			FVector SegmentEnd = Transform.TransformPosition(FRotator(0, -0.5f * Angle + (AngleStep * j), 0).RotateVector(ScaledAxis));
			Segments.Emplace(SegmentStart, SegmentEnd);
			SegmentStart = SegmentEnd;
		}
		Segments.Emplace(Center, SegmentStart);
		if (i > 0)
		{
			// Connect sector arc to previous arc
			for (int32 j = 0; j < SegmentCount; j++)
			{
				Segments.Emplace(Segments[LastArcStartIndex + j].Key, Segments[CurrentArcStartIndex + j].Key);
			}
			Segments.Emplace(Segments[LastArcStartIndex + SegmentCount - 1].Value, Segments[CurrentArcStartIndex + SegmentCount - 1].Value);
		}
		LastArcStartIndex = CurrentArcStartIndex;
	}
	return Segments;
}

bool FSphericalSector::IntersectsBox(const FBox2D& InBox) const
{
	const FVector ScaledAxis = FVector(FVector2D(Axis), 0).GetSafeNormal() * Radius;
	const FVector SectorStart = FRotator(0, 0.5f * Angle, 0).RotateVector(ScaledAxis);
	const FVector SectorEnd = FRotator(0, -0.5f * Angle, 0).RotateVector(ScaledAxis);
	const FVector2D SectorCenter(Center);
	const FVector2D SectorStartVector = FVector2D(SectorStart);
	const FVector2D SectorEndVector = FVector2D(SectorEnd);
	const float SectorRadiusSquared = Radius * Radius;

	// Test whether any cell corners are inside sector
	if (IsPointInsideSector(FVector2D(InBox.Min.X, InBox.Min.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, Angle) ||
		IsPointInsideSector(FVector2D(InBox.Max.X, InBox.Min.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, Angle) ||
		IsPointInsideSector(FVector2D(InBox.Max.X, InBox.Max.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, Angle) ||
		IsPointInsideSector(FVector2D(InBox.Min.X, InBox.Max.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, Angle))
	{
		return true;
	}

	// Test whether any sector point lies inside the cell bounds
	if (InBox.IsInside(SectorCenter) ||
		InBox.IsInside(SectorCenter + SectorStartVector) ||
		InBox.IsInside(SectorCenter + SectorEndVector))
	{
		return true;
	}

	// Test whether closest point on cell from center is inside sector
	if (IsPointInsideSector(InBox.GetClosestPointTo(SectorCenter), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, Angle))
	{
		return true;
	}

	return false;
}
