// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SplineComponent.h"
#include "WaterSplineMetadata.h"
#include "WaterSplineComponent.generated.h"

#if WITH_EDITOR
struct FOnWaterSplineDataChangedParams
{
	FOnWaterSplineDataChangedParams(const FPropertyChangedEvent& InPropertyChangedEvent = FPropertyChangedEvent(/*InProperty = */nullptr)) 
		: PropertyChangedEvent(InPropertyChangedEvent)
	{}

	/** Provides some additional context about how the water brush actor data has changed (property, type of change...) */
	FPropertyChangedEvent PropertyChangedEvent;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWaterSplineDataChanged, const FOnWaterSplineDataChangedParams&);
#endif // WITH_EDITOR

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WATER_API UWaterSplineComponent : public USplineComponent
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Defaults which are used to propagate values to spline points on instances of this in the world
	 */
	UPROPERTY(Category = Water, EditDefaultsOnly)
	FWaterSplineCurveDefaults WaterSplineDefaults;

	/** 
	 * This stores the last defaults propagated to spline points on an instance of this component 
	 *  Used to determine if spline points were modifed by users or if they exist at a current default value
	 */
	UPROPERTY()
	FWaterSplineCurveDefaults PreviousWaterSplineDefaults;
public:
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPie) override;

	/** Spline component interface */
	virtual USplineMetadata* GetSplinePointsMetadata() override;
	virtual const USplineMetadata* GetSplinePointsMetadata() const override;

	virtual TArray<ESplinePointType::Type> GetEnabledSplinePointTypes() const override;

	virtual bool AllowsSplinePointScaleEditing() const override { return false; }

#if WITH_EDITOR
	DECLARE_EVENT(UWaterSplineComponent, UE_DEPRECATED(5.1, "Use FOnWaterSplineDataChanged") FOnSplineDataChanged);
	UE_DEPRECATED(5.1, "Use OnWaterSplineDataChanged")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnSplineDataChanged& OnSplineDataChanged() { static FOnSplineDataChanged DeprecatedEvent; return DeprecatedEvent; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FOnWaterSplineDataChanged& OnWaterSplineDataChanged() { return WaterSplineDataChangedEvent; }

	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	
	void ResetSpline(const TArray<FVector>& Points);
	bool SynchronizeWaterProperties();
#endif // WITH_EDITOR

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void Serialize(FArchive& Ar) override;

private:
#if WITH_EDITOR
	FOnWaterSplineDataChanged WaterSplineDataChangedEvent;
#endif // WITH_EDITOR
};