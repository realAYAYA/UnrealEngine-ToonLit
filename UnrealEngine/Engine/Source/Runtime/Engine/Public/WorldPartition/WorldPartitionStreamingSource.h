// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Math/Color.h"
#include "Math/RandomStream.h"
#include "HAL/IConsoleManager.h"
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
	TArray<TPair<FVector, FVector>> BuildDebugMesh() const;

	/** Returns whether spherical sector intersects 2D box */
	bool IntersectsBox(const FBox2D& InBox) const;

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
	, LoadingRangeScale(1.f)
	, Radius(10000.0f)
	, bIsSector(false)
	, SectorAngle(360.0f)
	, Location(ForceInitToZero)
	, Rotation(ForceInitToZero)
	{}

	/* If True, streaming source shape radius is bound to loading range radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming)
	bool bUseGridLoadingRange;

	/* Applies a scale to the grid's loading range (used if bUseGridLoadingRange is True). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Streaming, meta = (EditCondition = "bUseGridLoadingRange"))
	float LoadingRangeScale;

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

	FString ToString() const;

	friend uint32 GetTypeHash(const FStreamingSourceShape& InShape)
	{
		uint32 Hash = GetTypeHash(InShape.bUseGridLoadingRange);
		Hash = HashCombine(Hash, GetTypeHash(InShape.LoadingRangeScale));
		Hash = HashCombine(Hash, GetTypeHash(InShape.Radius));
		Hash = HashCombine(Hash, GetTypeHash(InShape.bIsSector));
		Hash = HashCombine(Hash, GetTypeHash(InShape.SectorAngle));
		Hash = HashCombine(Hash, GetTypeHash(InShape.Location));
		Hash = HashCombine(Hash, GetTypeHash(InShape.Rotation.Pitch));
		Hash = HashCombine(Hash, GetTypeHash(InShape.Rotation.Yaw));
		Hash = HashCombine(Hash, GetTypeHash(InShape.Rotation.Roll));
		return Hash;
	}
};

UENUM(BlueprintType)
enum class EStreamingSourceTargetBehavior : uint8
{
	Include,
	Exclude
};

/** Helper class used to iterate over streaming source shapes. */
class FStreamingSourceShapeHelper
{
public:
	FORCEINLINE static bool IsSourceAffectingGrid(const TSet<FName>& InSourceTargetGrids, EStreamingSourceTargetBehavior InSourceTargetBehavior, FName InGridName)
	{
		switch (InSourceTargetBehavior)
		{
		case EStreamingSourceTargetBehavior::Include:
			return InSourceTargetGrids.IsEmpty() || InSourceTargetGrids.Contains(InGridName);
		case EStreamingSourceTargetBehavior::Exclude:
			return !InSourceTargetGrids.Contains(InGridName);
		default:
			checkNoEntry();
		}

		return false;
	}

	FORCEINLINE static void ForEachShape(float InGridLoadingRange, float InDefaultRadius, bool bInProjectIn2D, const FVector& InLocation, const FRotator& InRotation, const TArray<FStreamingSourceShape>& InShapes, TFunctionRef<void(const FSphericalSector&)> InOperation, float InExtraRadius = 0.f, float InExtraAngle = 0.f)
	{
		const FTransform Transform(bInProjectIn2D ? FRotator(0, InRotation.Yaw, 0) : InRotation, InLocation);
		if (InShapes.IsEmpty())
		{
			FSphericalSector LocalShape(FVector::ZeroVector, InDefaultRadius + InExtraRadius);
			if (LocalShape.IsValid())
			{
				InOperation(LocalShape.TransformBy(Transform));
			}
		}
		else
		{
			for (const FStreamingSourceShape& Shape : InShapes)
			{
				const FVector::FReal ShapeRadius = (Shape.bUseGridLoadingRange ? (InGridLoadingRange * Shape.LoadingRangeScale) : Shape.Radius) + InExtraRadius;
				const FVector::FReal ShapeAngle = Shape.bIsSector ? (Shape.SectorAngle + InExtraAngle) : 360.0f;
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
		, TargetBehavior(EStreamingSourceTargetBehavior::Include)
	{}

	FWorldPartitionStreamingQuerySource(const FVector& InLocation)
		: Location(InLocation)
		, Radius(0.f)
		, bUseGridLoadingRange(true)
		, bDataLayersOnly(false)
		, bSpatialQuery(true)
		, Rotation(ForceInitToZero)
		, TargetBehavior(EStreamingSourceTargetBehavior::Include)
	{}

	// Define Copy Constructor to avoid deprecation warnings
	FWorldPartitionStreamingQuerySource(const FWorldPartitionStreamingQuerySource& Other)
	{
		*this = Other;
	}

	FWorldPartitionStreamingQuerySource& operator=(const FWorldPartitionStreamingQuerySource& Other)
	{
		Location = Other.Location;
		Radius = Other.Radius;
		bUseGridLoadingRange = Other.bUseGridLoadingRange;
		DataLayers = Other.DataLayers;
		bDataLayersOnly = Other.bDataLayersOnly;
		bSpatialQuery = Other.bSpatialQuery;
		Rotation = Other.Rotation;
		TargetBehavior = Other.TargetBehavior;
		TargetGrids = Other.TargetGrids;
		Shapes = Other.Shapes;
		return *this;
	}

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
	
	/** Defines how TargetGrids will be applied to this streaming source. */
	EStreamingSourceTargetBehavior TargetBehavior;

	TSet<FName> TargetGrids;

	UE_DEPRECATED(5.4, "TargetHLODLayers is depredcated, use TargetGrids instead.")
	TSet<FSoftObjectPath> TargetHLODLayers;
	
	TArray<FStreamingSourceShape> Shapes;

	/** Helper method that iterates over all shapes. If none is provided, it will still pass a sphere shape using Radius or grid's loading range (see bUseGridLoadingRange). */
	FORCEINLINE void ForEachShape(float InGridLoadingRange, FName InGridName, bool bInProjectIn2D, TFunctionRef<void(const FSphericalSector&)> InOperation) const
	{
		if (bSpatialQuery && FStreamingSourceShapeHelper::IsSourceAffectingGrid(TargetGrids, TargetBehavior, InGridName))
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
struct FWorldPartitionStreamingSource
{
	FWorldPartitionStreamingSource()
		: Name(NAME_None)
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, TargetState(EStreamingSourceTargetState::Activated)
		, bBlockOnSlowLoading(false)
		, Priority(EStreamingSourcePriority::Default)
		, Velocity(FVector::Zero())
		, bUseVelocityContributionToCellsSorting(false)
		, DebugColor(ForceInit)
		, TargetBehavior(EStreamingSourceTargetBehavior::Include)
		, bReplay(false)
		, bRemote(false)
		, Hash2D(0)
		, Hash3D(0)
		, OldLocation(FVector::ZeroVector)
		, OldRotation(FRotator::ZeroRotator)
		, ExtraRadius(0)
		, ExtraAngle(0)
	{}

	FWorldPartitionStreamingSource(FName InName, const FVector& InLocation, const FRotator& InRotation, EStreamingSourceTargetState InTargetState, bool bInBlockOnSlowLoading, EStreamingSourcePriority InPriority, bool bRemote, FVector InVelocity = FVector::Zero())
		: Name(InName)
		, Location(InLocation)
		, Rotation(InRotation)
		, TargetState(InTargetState)
		, bBlockOnSlowLoading(bInBlockOnSlowLoading)
		, Priority(InPriority)
		, Velocity(InVelocity)
		, bUseVelocityContributionToCellsSorting(false)
		, DebugColor(ForceInit)
		, TargetBehavior(EStreamingSourceTargetBehavior::Include)
		, bReplay(false)
		, bRemote(bRemote)
		, Hash2D(0)
		, Hash3D(0)
		, OldLocation(InLocation)
		, OldRotation(InRotation)
		, ExtraRadius(0)
		, ExtraAngle(0)
	{}

	// Define Copy Constructor to avoid deprecation warnings
	FWorldPartitionStreamingSource(const FWorldPartitionStreamingSource& Other)
	{
		*this = Other;
	}

	FWorldPartitionStreamingSource& operator=(const FWorldPartitionStreamingSource& Other)
	{
		Name = Other.Name;
		Location = Other.Location;
		Rotation = Other.Rotation;
		TargetState = Other.TargetState;
		bBlockOnSlowLoading = Other.bBlockOnSlowLoading;
		Priority = Other.Priority;
		bUseVelocityContributionToCellsSorting = Other.bUseVelocityContributionToCellsSorting;
		Velocity = Other.Velocity;
		DebugColor = Other.DebugColor;
		TargetBehavior = Other.TargetBehavior;
		TargetGrids = Other.TargetGrids;
		Shapes = Other.Shapes;
		bReplay = Other.bReplay;
		bRemote = Other.bRemote;
		Hash2D = Other.Hash2D;
		Hash3D = Other.Hash3D;
		OldLocation = Other.OldLocation;
		OldRotation = Other.OldRotation;
		ExtraRadius = Other.ExtraRadius;
		ExtraAngle = Other.ExtraAngle;
		return *this;
	}
	
	FColor GetDebugColor() const
	{
		if (!DebugColor.ToPackedBGRA())
		{
			return FColor::MakeRedToGreenColorFromScalar(FRandomStream(Name).GetFraction());
		}

		return FColor(DebugColor.R, DebugColor.G, DebugColor.B, 255);
	}

	ENGINE_API void UpdateHash();
	uint32 GetHash(bool bInclude3DInformation = true) const { return bInclude3DInformation ? Hash3D : Hash2D; }

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
	FVector Velocity;
	
	/** Use velocity contribution to streaming cells priority streaming. */
	bool bUseVelocityContributionToCellsSorting;

	/** Color used for debugging. */
	FColor DebugColor;

	/** Defines how TargetGrids/TargetHLODLayers will be applied to this streaming source. */
	EStreamingSourceTargetBehavior TargetBehavior;

	/** When set, this will change how this streaming source is applied to the provided runtime streaming grids based on the TargetBehavior. */
	TSet<FName> TargetGrids;
	
	/** When set, this will change how this streaming source is applied to the provided HLODLayers based on the TargetBehavior. */
	UE_DEPRECATED(5.4, "TargetHLODLayers is depredcated, use TargetGrids instead.")
	TSet<FSoftObjectPath> TargetHLODLayers;

	/** Source internal shapes. When none are provided, a sphere is automatically used. It's radius is equal to grid's loading range and center equals source's location. */
	TArray<FStreamingSourceShape> Shapes;

	/** If true, this streaming source is from a replay recording */
	bool bReplay;

	/** If true, this streaming source is from a remote session */
	bool bRemote;

	/** Returns a box encapsulating all shapes. */
	FBox CalcBounds(float InGridLoadingRange, FName InGridName, bool bCalcIn2D = false) const
	{
		FBox OutBounds(ForceInit);
		ForEachShape(InGridLoadingRange, InGridName, bCalcIn2D, [&OutBounds](const FSphericalSector& Sector)
		{
			OutBounds += Sector.CalcBounds();
		});
		return OutBounds;
	}

	/** Helper method that iterates over all shapes. If none is provided, it will still pass a sphere shape using grid's loading range. */
	void ForEachShape(float InGridLoadingRange, FName InGridName, bool bInProjectIn2D, TFunctionRef<void(const FSphericalSector&)> InOperation) const
	{
		if (FStreamingSourceShapeHelper::IsSourceAffectingGrid(TargetGrids, TargetBehavior, InGridName))
		{
			FStreamingSourceShapeHelper::ForEachShape(InGridLoadingRange, InGridLoadingRange, bInProjectIn2D, Location, Rotation, Shapes, InOperation, ExtraRadius, ExtraAngle);
		}
	}

	FString ToString() const;

	static int32 GetLocationQuantization() { return LocationQuantization; }
	static int32 GetRotationQuantization() { return RotationQuantization; }

private:
	static ENGINE_API int32 LocationQuantization;
	static ENGINE_API int32 RotationQuantization;
	static ENGINE_API FAutoConsoleVariableRef CVarLocationQuantization;
	static ENGINE_API FAutoConsoleVariableRef CVarRotationQuantization;

	/** Hash of streaming source (used to detect changes) */
	uint32 Hash2D;
	uint32 Hash3D;

	/** Source values used for hash computations. */
	FVector OldLocation;
	FRotator OldRotation;

	/** Used internally for server streaming */
	float ExtraRadius;
	float ExtraAngle;

	friend struct FSetStreamingSourceExtraRadius;
	friend struct FSetStreamingSourceExtraAngle;
};

struct FSetStreamingSourceExtraRadius
{
private:
	FSetStreamingSourceExtraRadius(FWorldPartitionStreamingSource& InStreamingSource, float InExtraRadius)
	{
		if (ensure(InExtraRadius >= 0.f))
		{
			InStreamingSource.ExtraRadius = InExtraRadius;
		}
	}

	friend class UWorldPartitionSubsystem;
};

struct FSetStreamingSourceExtraAngle
{
private:
	FSetStreamingSourceExtraAngle(FWorldPartitionStreamingSource& InStreamingSource, float InExtraAngle)
	{
		if (ensure(InExtraAngle >= 0.f))
		{
			InStreamingSource.ExtraAngle = InExtraAngle;
		}
	}

	friend class UWorldPartitionSubsystem;
};

/**
 * Interface for world partition streaming sources
 */
struct IWorldPartitionStreamingSourceProvider
{
	virtual bool GetStreamingSource(FWorldPartitionStreamingSource& StreamingSource) const
	{
		return false;
	}

	virtual bool GetStreamingSources(TArray<FWorldPartitionStreamingSource>& StreamingSources) const
	{
		FWorldPartitionStreamingSource StreamingSource;
		if (GetStreamingSource(StreamingSource))
		{
			StreamingSources.Add(StreamingSource);
			return true;
		}
		return false;
	}

	virtual const UObject* GetStreamingSourceOwner() const { return nullptr; }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
