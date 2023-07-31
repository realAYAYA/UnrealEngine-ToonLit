// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/BaseMeshProcessingTool.h"
#include "SmoothMeshTool.generated.h"


UENUM()
enum class ESmoothMeshToolSmoothType : uint8
{
	/** Iterative smoothing with N iterations */
	Iterative UMETA(DisplayName = "Fast Iterative"),

	/** Implicit smoothing, produces smoother output and does a better job at preserving UVs, but can be very slow on large meshes */
	Implicit UMETA(DisplayName = "Fast Implicit"),

	/** Iterative implicit-diffusion smoothing with N iterations */
	Diffusion UMETA(DisplayName = "Iterative Diffusion")
};



/** PropertySet for properties affecting the Smoother. */
UCLASS()
class MESHMODELINGTOOLSEXP_API USmoothMeshToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Type of smoothing to apply */
	UPROPERTY(EditAnywhere, Category = SmoothingType)
	ESmoothMeshToolSmoothType SmoothingType = ESmoothMeshToolSmoothType::Iterative;
};



/** Properties for Iterative Smoothing */
UCLASS()
class MESHMODELINGTOOLSEXP_API UIterativeSmoothProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Amount of smoothing allowed per step. Smaller steps will avoid things like collapse of small/thin features. */
	UPROPERTY(EditAnywhere, Category = IterativeSmoothingOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float SmoothingPerStep = 0.8f;

	/** Number of Smoothing iterations */
	UPROPERTY(EditAnywhere, Category = IterativeSmoothingOptions, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	int32 Steps = 10;

	/** If this is false, the smoother will try to reshape the triangles to be more regular, which will distort UVs */
	UPROPERTY(EditAnywhere, Category = IterativeSmoothingOptions)
	bool bSmoothBoundary = true;
};



/** Properties for Diffusion Smoothing */
UCLASS()
class MESHMODELINGTOOLSEXP_API UDiffusionSmoothProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Amount of smoothing allowed per step. Smaller steps will avoid things like collapse of small/thin features. */
	UPROPERTY(EditAnywhere, Category = DiffusionSmoothingOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float SmoothingPerStep = 0.8f;

	/** Number of Smoothing iterations */
	UPROPERTY(EditAnywhere, Category = DiffusionSmoothingOptions, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	int32 Steps = 1;

	/** If this is false, the smoother will try to reshape the triangles to be more regular, which will distort UVs */
	UPROPERTY(EditAnywhere, Category = DiffusionSmoothingOptions)
	bool bPreserveUVs = true;
};




/** Properties for Implicit smoothing */
UCLASS()
class MESHMODELINGTOOLSEXP_API UImplicitSmoothProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Smoothing speed */
	//UPROPERTY(EditAnywhere, Category = ImplicitSmoothing, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	UPROPERTY()
	float SmoothSpeed = 0.1f;

	/** Desired Smoothness. This is not a linear quantity, but larger numbers produce smoother results */
	UPROPERTY(EditAnywhere, Category = ImplicitSmoothingOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "100.0"))
	float Smoothness = 0.2f;

	/** If this is false, the smoother will try to reshape the triangles to be more regular, which will distort UVs */
	UPROPERTY(EditAnywhere, Category = ImplicitSmoothingOptions)
	bool bPreserveUVs = true;

	/** Magic number that allows you to try to correct for shrinking caused by smoothing */
	UPROPERTY(EditAnywhere, Category = ImplicitSmoothingOptions, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "10.0"))
	float VolumeCorrection = 0.0f;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API USmoothWeightMapSetProperties : public UWeightMapSetProperties
{
	GENERATED_BODY()
public:

	/** Fractional Minimum Smoothing Parameter in World Units, for Weight Map values of zero */
	UPROPERTY(EditAnywhere, Category = WeightMap, AdvancedDisplay, meta = (UIMin = "0.0", UIMax = "1.0", DisplayPriority = 5))
	float MinSmoothMultiplier = 0.0f;
};




/**
 * Mesh Smoothing Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USmoothMeshTool : public UBaseMeshProcessingTool
{
	GENERATED_BODY()

public:
	USmoothMeshTool();

	virtual void InitializeProperties() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	virtual bool RequiresInitialVtxNormals() const { return true; }
	virtual bool HasMeshTopologyChanged() const override;

	virtual FText GetToolMessageString() const override;
	virtual FText GetAcceptTransactionName() const override;

protected:
	UPROPERTY()
	TObjectPtr<USmoothMeshToolProperties> SmoothProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UIterativeSmoothProperties> IterativeProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UDiffusionSmoothProperties> DiffusionProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UImplicitSmoothProperties> ImplicitProperties = nullptr;

	UPROPERTY()
	TObjectPtr<USmoothWeightMapSetProperties> WeightMapProperties = nullptr;
};




/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USmoothMeshToolBuilder : public UBaseMeshProcessingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UBaseMeshProcessingTool* MakeNewToolInstance(UObject* Outer) const {
		return NewObject<USmoothMeshTool>(Outer);
	}
};
