// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGTimeSlicedElementBase.h"

#include "Containers/Array.h"

#include "PCGSelfPruning.generated.h"

UENUM()
enum class EPCGSelfPruningType : uint8
{
	LargeToSmall,
	SmallToLarge,
	AllEqual,
	None,
	RemoveDuplicates
};

USTRUCT(BlueprintType)
struct FPCGSelfPruningParameters
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGSelfPruningType PruningType = EPCGSelfPruningType::LargeToSmall;

	/** By default, it will prune according to the extents of each point, but you can provide another comparison like Density, or a dynamic attribute. Only support numeric values or vector (vector will be reduced to its squared length). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "PruningType == EPCGSelfPruningType::LargeToSmall || PruningType == EPCGSelfPruningType::SmallToLarge", PCG_Overridable))
	FPCGAttributePropertyInputSelector ComparisonSource;

	/** Similarity factor to consider 2 points "equal". (For example, if a point extents squared length is 10 and factor is 0.25, all points between 7.5 and 12.5 will be considered "the same"). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = 0.0f, EditCondition = "PruningType == EPCGSelfPruningType::LargeToSmall || PruningType == EPCGSelfPruningType::SmallToLarge", PCG_Overridable))
	float RadiusSimilarityFactor = 0.25f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bRandomizedPruning = true;
};

namespace PCGSelfPruningElement
{
	// all points are in contiguous memory so this will create a bitset to just look up the index to set/test a bit
	// this should reduce the 'set' cost to O[1]
	struct FPointBitSet
	{
		TArray<uint32> Bits;
		const FPCGPoint* FirstPoint = nullptr;

		void Initialize(const TArray<FPCGPoint>& Points);
		void Add(const FPCGPoint* Point);
		bool Contains(const FPCGPoint* Point);
	};

	struct FIterationState
	{
		const UPCGPointData* InputPointData = nullptr;
		UPCGPointData* OutputPointData = nullptr;
		TArray<FPCGPointRef> SortedPoints;
		FPointBitSet ExcludedPoints;
		FPointBitSet ExclusionPoints;
		int32 CurrentPointIndex = 0;
		bool bSortDone = false;
	};

	/** Will do the self pruning on all inputs in the context. Blocking call (no time slicing). Be careful, it can be very costly. */
	void Execute(FPCGContext* Context, EPCGSelfPruningType PruningType, float RadiusSimilarityFactor, bool bRandomizedPruning);

	/** Will do the self pruning on all inputs in the context. Blocking call (no time slicing). Be careful, it can be very costly. */
	PCG_API void Execute(FPCGContext* Context, const FPCGSelfPruningParameters& InParameters);

	/** Will execute self pruning according to the info from FIterationState. Time sliced call, will return true if it is done. */
	PCG_API bool ExecuteSlice(FIterationState& InState, const FPCGSelfPruningParameters& InParameters, FPCGContext* InOptionalContext = nullptr);
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup=(Procedural))
class UPCGSelfPruningSettings : public UPCGSettings
{
	GENERATED_BODY()

	UPCGSelfPruningSettings();

	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties, PCG_Overridable))
	FPCGSelfPruningParameters Parameters;

#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SelfPruning")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSelfPruningSettings", "NodeTitle", "Self Pruning"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	EPCGSelfPruningType PruningType_DEPRECATED = EPCGSelfPruningType::LargeToSmall;

	UPROPERTY()
	float RadiusSimilarityFactor_DEPRECATED = 0.25f;

	UPROPERTY()
	bool bRandomizedPruning_DEPRECATED = true;
#endif // WITH_EDITORONLY_DATA
};

class FPCGSelfPruningElement : public TPCGTimeSlicedElementBase<PCGTimeSlice::FEmptyStruct, PCGSelfPruningElement::FIterationState>
{
protected:
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
