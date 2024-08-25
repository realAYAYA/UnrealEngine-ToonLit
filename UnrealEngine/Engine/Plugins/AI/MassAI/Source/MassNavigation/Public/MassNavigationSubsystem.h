// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "HierarchicalHashGrid2D.h"
#include "MassSubsystemBase.h"
#include "MassNavigationSubsystem.generated.h"

UENUM()
enum class EMassNavigationObstacleFlags : uint8
{
	None			= 0,
	HasColliderData = 1 << 0,
};
ENUM_CLASS_FLAGS(EMassNavigationObstacleFlags)

struct FMassNavigationObstacleItem
{
	bool operator==(const FMassNavigationObstacleItem& Other) const
	{
		return Entity == Other.Entity;
	}

	FMassEntityHandle Entity;
	EMassNavigationObstacleFlags ItemFlags = EMassNavigationObstacleFlags::None;
};

typedef THierarchicalHashGrid2D<2, 4, FMassNavigationObstacleItem> FNavigationObstacleHashGrid2D;	// 2 levels of hierarchy, 4 ratio between levels

UCLASS()
class MASSNAVIGATION_API UMassNavigationSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:
	UMassNavigationSubsystem();

	const FNavigationObstacleHashGrid2D& GetObstacleGrid() const { return AvoidanceObstacleGrid; }
	FNavigationObstacleHashGrid2D& GetObstacleGridMutable() { return AvoidanceObstacleGrid; }

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem END

	FNavigationObstacleHashGrid2D AvoidanceObstacleGrid;
};

template<>
struct TMassExternalSubsystemTraits<UMassNavigationSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};
