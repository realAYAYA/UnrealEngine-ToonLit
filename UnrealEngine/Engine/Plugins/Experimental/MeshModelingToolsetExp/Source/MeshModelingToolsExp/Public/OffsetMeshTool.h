// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/BaseMeshProcessingTool.h"
#include "PropertySets/WeightMapSetProperties.h"
#include "OffsetMeshTool.generated.h"


UENUM()
enum class EOffsetMeshToolOffsetType : uint8
{
	/** Iterative Offsetting with N iterations */
	Iterative UMETA(DisplayName = "Iterative"),

	/** Implicit Offsetting, produces smoother output and does a better job at preserving UVs, but can be very slow on large meshes */
	Implicit UMETA(DisplayName = "Implicit")
};



/** Base properties of Offset */
UCLASS()
class MESHMODELINGTOOLSEXP_API UOffsetMeshToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Type of Offsetting to apply */
	UPROPERTY(EditAnywhere, Category = Offset)
	EOffsetMeshToolOffsetType OffsetType = EOffsetMeshToolOffsetType::Iterative;

	/** Offset Distance in World Units */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (UIMin = "-100.0", UIMax = "100.0", ClampMin = "-10000.0", ClampMax = "100000.0"))
	float Distance = 1.0f;

	/** If true, create a thickened shell, instead of only moving the input vertices */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "OffsetType == EOffsetMeshToolOffsetType::Iterative") )
	bool bCreateShell = false;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UOffsetWeightMapSetProperties : public UWeightMapSetProperties
{
	GENERATED_BODY()
public:

	/** Minimum Offset Distance in World Units, for Weight Map values of zero (clamped to Distance) */
	UPROPERTY(EditAnywhere, Category = WeightMap, meta = (UIMin = "-100.0", UIMax = "100.0", ClampMin = "-10000.0", ClampMax = "100000.0", DisplayPriority = 5))
	float MinDistance = 1.0f;
};


/** Properties for Iterative Offsetting */
UCLASS()
class MESHMODELINGTOOLSEXP_API UIterativeOffsetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Number of Offsetting iterations */
	UPROPERTY(EditAnywhere, Category = IterativeOffsetOptions, meta = (UIMin = "1", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	int32 Steps = 10;

	/** Control whether the boundary is allowed to move */
	UPROPERTY(EditAnywhere, Category = IterativeOffsetOptions)
	bool bOffsetBoundaries = true;

	/** Amount of smoothing applied per Offset step */
	UPROPERTY(EditAnywhere, Category = IterativeOffsetOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float SmoothingPerStep = 0.0f;

	/** Reproject smooth vertices onto non-smoothed Offset Surface at each step (expensive but better-preserves uniform distance) */
	UPROPERTY(EditAnywhere, Category = IterativeOffsetOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition="SmoothingPerStep > 0"))
	bool bReprojectSmooth = false;
};


/** Properties for Implicit Offsetting */
UCLASS()
class MESHMODELINGTOOLSEXP_API UImplicitOffsetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** How tightly we should constrain the constrained implicit offset to the explicit offset */
	UPROPERTY(EditAnywhere, Category = ImplicitOffsettingOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "100.0"))
	float Smoothness = 0.2f;

	/** If this is false, triangles will be reshaped to be more regular, which will distort UVs */
	UPROPERTY(EditAnywhere, Category = ImplicitOffsettingOptions)
	bool bPreserveUVs = true;
};






/**
 * Mesh Offsetting Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UOffsetMeshTool : public UBaseMeshProcessingTool
{
	GENERATED_BODY()

public:
	UOffsetMeshTool();

	virtual void InitializeProperties() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	virtual bool RequiresInitialVtxNormals() const { return true; }
	virtual bool RequiresInitialBoundaryLoops() const { return true; }
	virtual bool HasMeshTopologyChanged() const override;

	virtual FText GetToolMessageString() const override;
	virtual FText GetAcceptTransactionName() const override;

protected:
	UPROPERTY()
	TObjectPtr<UOffsetMeshToolProperties> OffsetProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UIterativeOffsetProperties> IterativeProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UImplicitOffsetProperties> ImplicitProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UOffsetWeightMapSetProperties> WeightMapProperties = nullptr;
};




/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UOffsetMeshToolBuilder : public UBaseMeshProcessingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UBaseMeshProcessingTool* MakeNewToolInstance(UObject* Outer) const {
		return NewObject<UOffsetMeshTool>(Outer);
	}
};
