// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Math/RandomStream.h"
#include "WorldPartitionStreamingSource.generated.h"

/** See https://en.wikipedia.org/wiki/Spherical_sector. */
class FSphericalSector
{
public:
	using FReal = FVector::FReal;

	/** Creates and initializes a new spherical sector. */
	FSphericalSector(EForceInit)
		: Center(ForceInit)
		, Radius(0.0f)
		, Axis(ForceInit)
	{
		SetAsSphere();
	}

	/** Creates and initializes a spherical sector using given parameters. */
	FSphericalSector(FVector InCenter, FReal InRadius, FVector InAxis = FVector::ForwardVector, FReal InAngle = 0)
		: Center(InCenter)
		, Radius(InRadius)
	{
		SetAngle(InAngle);
		SetAxis(InAxis);
	}

	void SetCenter(const FVector& InCenter) { Center = InCenter; }
	const FVector& GetCenter() const { return Center; }

	void SetRadius(FReal InRadius) { Radius = InRadius; }
	FReal GetRadius() const { return Radius; }

	void SetAngle(FReal InAngle) { Angle = (InAngle <= 0.0f || InAngle > 360.0f) ? 360.0f : InAngle; }
	FReal GetAngle() const { return Angle; }

	void SetAxis(const FVector& InAxis) { Axis = InAxis.GetSafeNormal(); }
	FVector GetAxis() const { return Axis; }
	FVector GetScaledAxis() const { return Axis * Radius; }

	void SetAsSphere() { SetAngle(360.0f); }
	bool IsSphere() const { return FMath::IsNearlyEqual(Angle, (FReal)360.0); }

	bool IsNearlyZero() const { return FMath::IsNearlyZero(Radius) || Axis.IsNearlyZero() || FMath::IsNearlyZero(Angle); }
	bool IsValid() const { return !IsNearlyZero(); }

	FBox CalcBounds() const
	{
		const FVector Offset(Radius);
		return FBox(Center - Offset, Center + Offset);
	}

	/** Get result of Transforming spherical sector with transform. */
	FSphericalSector TransformBy(const FTransform& M) const
	{
		FSphericalSector Result(M.TransformPosition(Center), M.GetMaximumAxisScale() * Radius, M.TransformVector(Axis), Angle);
		return Result;
	}

	/** Helper method that builds a list of debug display segments */
	TArray<TPair<FVector, FVector>> BuildDebugMesh() const
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

private:
	/** Sphere center point. */
	FVector Center;

	/** Sphere radius. */
	FReal Radius;

	/** Sector axis (direction). */
	FVector Axis;

	/** Optional sector angle in degree (360 = regular sphere). */
	FReal Angle;
};

USTRUCT(BlueprintType)
struct FStreamingSourceShape
{
	GENERATED_BODY()

	FStreamingSourceShape()
	: bUseGridLoadingRange(true)
	, Radius(10000.0f)
	, bIsSector(false)
	, SectorAngle(360.0f)
	, Location(ForceInitToZero)
	, Rotation(ForceInitToZero)
	{}

	/* If True, streaming source shape radius is bound to loading range radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming)
	bool bUseGridLoadingRange;

	/* Custom streaming source shape radius (not used if bUseGridLoadingRange is True). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming, meta = (EditCondition = "!bUseGridLoadingRange"))
	float Radius;

	/* Whether the source shape is a spherical sector instead of a regular sphere source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming)
	bool bIsSector;

	/* Shape's spherical sector angle in degree (not used if bIsSector is False). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming, meta = (EditCondition = "bIsSector", ClampMin = 0, ClampMax = 360))
	float SectorAngle;

	/* Streaming source shape location (local to streaming source). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming)
	FVector Location;

	/* Streaming source shape rotation (local to streaming source). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming)
	FRotator Rotation;
};

/** Helper class used to iterate over streaming source shapes. */
class FStreamingSourceShapeHelper
{
public:

	FORCEINLINE static bool IsSourceAffectingGrid(FName InSourceTargetGrid, const FSoftObjectPath& InSourceTargetHLODLayer, FName InGridName, const FSoftObjectPath& InGridHLODLayer)
	{
		if ((InSourceTargetGrid.IsNone() && InSourceTargetHLODLayer.IsNull()) ||
			(InSourceTargetHLODLayer.IsNull() && (InSourceTargetGrid == InGridName)) ||
			(!InSourceTargetHLODLayer.IsNull() && (InSourceTargetHLODLayer == InGridHLODLayer)))
		{
			return true;
		}
		return false;
	}
	FORCEINLINE static void ForEachShape(float InGridLoadingRange, float InDefaultRadius, bool bInProjectIn2D, const FVector& InLocation, const FRotator& InRotation, const TArray<FStreamingSourceShape>& InShapes, TFunctionRef<void(const FSphericalSector&)> InOperation)
	{
		const FTransform Transform(bInProjectIn2D ? FRotator(0, InRotation.Yaw, 0) : InRotation, InLocation);
		if (InShapes.IsEmpty())
		{
			FSphericalSector LocalShape(FVector::ZeroVector, InDefaultRadius);
			if (LocalShape.IsValid())
			{
				InOperation(LocalShape.TransformBy(Transform));
			}
		}
		else
		{
			for (const FStreamingSourceShape& Shape : InShapes)
			{
				const FVector::FReal ShapeRadius = Shape.bUseGridLoadingRange ? InGridLoadingRange : Shape.Radius;
				const FVector::FReal ShapeAngle = Shape.bIsSector ? Shape.SectorAngle : 360.0f;
				const FVector ShapeAxis = bInProjectIn2D ? FRotator(0, Shape.Rotation.Yaw, 0).Vector() : Shape.Rotation.Vector();
				FSphericalSector LocalShape(bInProjectIn2D ? FVector(Shape.Location.X, Shape.Location.Y, 0) : Shape.Location, ShapeRadius, ShapeAxis, ShapeAngle);
				if (LocalShape.IsValid())
				{
					InOperation(LocalShape.TransformBy(Transform));
				}
			}
		}
	}
};

/**
 * Streaming Source Target State
 */
UENUM()
enum class EStreamingSourceTargetState : uint8
{
	Loaded,
	Activated
};

inline const TCHAR* GetStreamingSourceTargetStateName(EStreamingSourceTargetState StreamingSourceTargetState)
{
	switch(StreamingSourceTargetState)

	{
	case EStreamingSourceTargetState::Loaded: return TEXT("Loaded");
	case EStreamingSourceTargetState::Activated: return TEXT("Activated");
	default: check(0);
	}
	return TEXT("Invalid");
}

/**
 * Structure containing all properties required to query a streaming state
 */
USTRUCT(BlueprintType)
struct FWorldPartitionStreamingQuerySource
{
	GENERATED_USTRUCT_BODY()

	FWorldPartitionStreamingQuerySource()
		: Location(FVector::ZeroVector)
		, Radius(0.f)
		, bUseGridLoadingRange(true)
		, bDataLayersOnly(false)
		, bSpatialQuery(true)
		, Rotation(ForceInitToZero)
		, TargetGrid(NAME_None)
	{}

	FWorldPartitionStreamingQuerySource(const FVector& InLocation)
		: Location(InLocation)
		, Radius(0.f)
		, bUseGridLoadingRange(true)
		, bDataLayersOnly(false)
		, bSpatialQuery(true)
		, Rotation(ForceInitToZero)
		, TargetGrid(NAME_None)
	{}

	/* Location to query. (not used if bSpatialQuery is false) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Query")
	FVector Location;

	/* Radius to query. (not used if bSpatialQuery is false) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Query")
	float Radius;

	/* If True, Instead of providing a query radius, query can be bound to loading range radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Query")
	bool bUseGridLoadingRange;

	/* Optional list of data layers to specialize the query. If empty only non data layer cells will be returned by the query. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Query")
	TArray<FName> DataLayers;

	/* If True, Only cells that are in a data layer found in DataLayers property will be returned by the query. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Query")
	bool bDataLayersOnly;

	/* If False, Location/Radius will not be used to find the cells. Only AlwaysLoaded cells will be returned by the query. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Query")
	bool bSpatialQuery;

	/* Reserved settings used by UWorldPartitionStreamingSourceComponent::IsStreamingCompleted. */
	FRotator Rotation;
	FName TargetGrid;
	FSoftObjectPath TargetHLODLayer;
	TArray<FStreamingSourceShape> Shapes;

	/** Helper method that iterates over all shapes. If none is provided, it will still pass a sphere shape using Radius or grid's loading range (see bUseGridLoadingRange). */
	FORCEINLINE void ForEachShape(float InGridLoadingRange, FName InGridName, const FSoftObjectPath& InGridHLODLayer, bool bInProjectIn2D, TFunctionRef<void(const FSphericalSector&)> InOperation) const
	{
		if (!bSpatialQuery)
		{
			return;
		}

		if (FStreamingSourceShapeHelper::IsSourceAffectingGrid(TargetGrid, TargetHLODLayer, InGridName, InGridHLODLayer))
		{
			FStreamingSourceShapeHelper::ForEachShape(InGridLoadingRange, bUseGridLoadingRange ? InGridLoadingRange : Radius, bInProjectIn2D, Location, Rotation, Shapes, InOperation);
		}
	}
};

/**
 * Streaming Source Priority
 */
UENUM(BlueprintType)
enum class EStreamingSourcePriority : uint8
{
	Highest = 0,
	High = 64,
	Normal = 128,
	Low = 192,
	Lowest = 255,
	Default = Normal
};

/**
 * Structure containing all properties required to stream from a source
 */
struct ENGINE_API FWorldPartitionStreamingSource
{
	FWorldPartitionStreamingSource()
		: bBlockOnSlowLoading(false)
		, Priority(EStreamingSourcePriority::Default)
		, Velocity(0.f)
		, DebugColor(ForceInit)
		, TargetGrid(NAME_None)
		, bReplay(false)
		, bRemote(false)
	{}

	FWorldPartitionStreamingSource(FName InName, const FVector& InLocation, const FRotator& InRotation, EStreamingSourceTargetState InTargetState, bool bInBlockOnSlowLoading, EStreamingSourcePriority InPriority, bool bRemote, float InVelocity = 0.f)
		: Name(InName)
		, Location(InLocation)
		, Rotation(InRotation)
		, TargetState(InTargetState)
		, bBlockOnSlowLoading(bInBlockOnSlowLoading)
		, Priority(InPriority)
		, Velocity(InVelocity)
		, DebugColor(ForceInit)
		, TargetGrid(NAME_None)
		, bReplay(false)
		, bRemote(bRemote)
	{}

	FColor GetDebugColor() const
	{
		if (!DebugColor.ToPackedBGRA())
		{
			return FColor::MakeRedToGreenColorFromScalar(FRandomStream(Name).GetFraction());
		}

		return FColor(DebugColor.R, DebugColor.G, DebugColor.B, 255);
	}

	/** Source unique name. */
	FName Name;

	/** Source location. */
	FVector Location;
	
	/** Source orientation (can impact streaming cell prioritization). */
	FRotator Rotation;

	/** Target streaming state. */
	EStreamingSourceTargetState TargetState;

	/** Whether this source will be considered when world partition detects slow loading and waits for cell streaming to complete. */
	bool bBlockOnSlowLoading;

	/** Streaming source priority. */
	EStreamingSourcePriority Priority;

	/** Source velocity (computed automatically). */
	float Velocity;

	/** Color used for debugging. */
	FColor DebugColor;

	/** When set, will only affect streaming on the provided target runtime streaming grid. */
	FName TargetGrid;

	/** When set, will only affect streaming on HLODs associated to the provided target HLODLayer. */
	FSoftObjectPath TargetHLODLayer;

	/** Source internal shapes. When none are provided, a sphere is automatically used. It's radius is equal to grid's loading range and center equals source's location. */
	TArray<FStreamingSourceShape> Shapes;

	/** If true, this streaming source is from a replay recording */
	bool bReplay;

	/** If true, this streaming source is from a remote session */
	bool bRemote;

	/** Returns a box encapsulating all shapes. */
	FORCEINLINE FBox CalcBounds(float InGridLoadingRange, FName InGridName, const FSoftObjectPath& InGridHLODLayer, bool bCalcIn2D = false) const
	{
		FBox OutBounds(ForceInit);
		ForEachShape(InGridLoadingRange, InGridName, InGridHLODLayer, bCalcIn2D, [&OutBounds](const FSphericalSector& Sector)
		{
			OutBounds += Sector.CalcBounds();
		});
		return OutBounds;
	}

	/** Helper method that iterates over all shapes. If none is provided, it will still pass a sphere shape using grid's loading range. */
	FORCEINLINE void ForEachShape(float InGridLoadingRange, FName InGridName, const FSoftObjectPath& InGridHLODLayer, bool bInProjectIn2D, TFunctionRef<void(const FSphericalSector&)> InOperation) const
	{
		if (FStreamingSourceShapeHelper::IsSourceAffectingGrid(TargetGrid, TargetHLODLayer, InGridName, InGridHLODLayer))
		{
			FStreamingSourceShapeHelper::ForEachShape(InGridLoadingRange, InGridLoadingRange, bInProjectIn2D, Location, Rotation, Shapes, InOperation);
		}
	}

	FString ToString() const
	{
		const FVector Direction = Rotation.Euler();
		return FString::Printf(
			TEXT("Priority: %d | %s | %s | %s | Pos: X=%lld,Y=%lld,Z=%lld | Rot: X=%d,Y=%d,Z=%d | Vel: %3.2f m/s (%d mph)"), 
			Priority, 
			bRemote ? TEXT("Remote") : TEXT("Local"),
			GetStreamingSourceTargetStateName(TargetState),
			bBlockOnSlowLoading ? TEXT("Blocking") : TEXT("NonBlocking"),
			(int64)Location.X, (int64)Location.Y, (int64)Location.Z, 
			(int32)Direction.X, (int32)Direction.Y, (int32)Direction.Z, 
			Velocity, 
			(int32)(Velocity*2.23694f)
		);
	}
};

/**
 * Interface for world partition streaming sources
 */
struct ENGINE_API IWorldPartitionStreamingSourceProvider
{
	virtual bool GetStreamingSource(FWorldPartitionStreamingSource& StreamingSource) const = 0;
};