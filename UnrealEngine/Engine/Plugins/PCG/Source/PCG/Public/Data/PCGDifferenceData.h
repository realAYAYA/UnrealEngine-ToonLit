// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "PCGDifferenceData.generated.h"

struct FPropertyChangedEvent;

class UPCGUnionData;

UENUM()
enum class EPCGDifferenceDensityFunction : uint8
{
	Minimum,
	ClampedSubstraction UMETA(DisplayName = "Clamped Subtraction"),
	Binary
};

UENUM()
enum class EPCGDifferenceMode : uint8
{
	Inferred,
	Continuous,
	Discrete
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGDifferenceData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void Initialize(const UPCGSpatialData* InData);

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void AddDifference(const UPCGSpatialData* InDifference);

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetDensityFunction(EPCGDifferenceDensityFunction InDensityFunction);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	bool bDiffMetadata = true;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ~Begin UObject interface
#if WITH_EDITOR
	virtual void PostLoad();
#endif
	// ~End UObject interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Spatial; }
	virtual void VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const override;

protected:
	virtual FPCGCrc ComputeCrc(bool bFullDataCrc) const override;
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

public:
	//~Begin UPCGSpatialData interface
	virtual int GetDimension() const override;
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override;
	virtual const UPCGSpatialData* FindFirstConcreteShapeFromNetwork() const override { return GetSource() ? GetSource()->FindFirstConcreteShapeFromNetwork() : nullptr; }

protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	TObjectPtr<const UPCGSpatialData> Source = nullptr;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	TObjectPtr<const UPCGSpatialData> Difference = nullptr;

	UPROPERTY()
	TObjectPtr<UPCGUnionData> DifferencesUnion = nullptr;

	UPROPERTY(BlueprintSetter = SetDensityFunction, EditAnywhere, Category = Settings)
	EPCGDifferenceDensityFunction DensityFunction = EPCGDifferenceDensityFunction::Minimum;

#if WITH_EDITOR
	inline const UPCGSpatialData* GetSource() const { return RawPointerSource; }
	inline const UPCGSpatialData* GetDifference() const { return RawPointerDifference; }
	inline UPCGUnionData* GetDifferencesUnion() const { return RawPointerDifferencesUnion; }
#else
	inline const UPCGSpatialData* GetSource() const { return Source.Get(); }
	inline const UPCGSpatialData* GetDifference() const { return Difference.Get(); }
	inline UPCGUnionData* GetDifferencesUnion() const { return DifferencesUnion.Get(); }
#endif

#if WITH_EDITOR
private:
	// Cached pointers to avoid dereferencing object pointer which does access tracking and supports lazy loading, and can come with substantial
	// overhead (add trace marker to FObjectPtr::Get to see).
	const UPCGSpatialData* RawPointerSource = nullptr;
	const UPCGSpatialData* RawPointerDifference = nullptr;
	UPCGUnionData* RawPointerDifferencesUnion = nullptr;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
