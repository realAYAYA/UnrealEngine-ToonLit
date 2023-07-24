// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGPin.h"
#include "PCGSettings.h"

#include "Async/Future.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshSamplingFunctions.h"
#include "Templates/UniquePtr.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

#include <atomic>

#include "PCGMeshSampler.generated.h"

class FProgressCancel;
class UDynamicMesh;
class UPCGPointData;
class UStaticMesh;

UENUM(BlueprintType)
enum class EPCGMeshSamplingMethod
{
	/** Sample one point (at the center) of each triangle of the mesh. */
	OnePointPerTriangle,

	/** Sample one point per vertex on the mesh. */
	OnePointPerVertex,

	/** Use Poisson sampling to sample points on the mesh. Can be expensive and therefore it is not framebound. */
	PoissonSampling
};

/**
* Sample points on a mesh
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGGEOMETRYSCRIPTINTEROP_API UPCGMeshSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGMeshSamplerSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MeshSampler")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGMeshSamplerSettings", "NodeTitle", "Mesh Sampler"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGMeshSamplingMethod SamplingMethod = EPCGMeshSamplingMethod::OnePointPerTriangle;

	/** Soft Object Path to the mesh to sample from. Will be loaded. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable, AllowedClasses = "/Script/Engine.StaticMesh", DisplayName = "Static Mesh"))
	FSoftObjectPath StaticMeshPath;

	/** In "One Point Per Vertex" option, will assign point density from the red component of the vertex color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Per-Vertex Options", meta = (PCG_Overridable, EditCondition = "SamplingMethod == EPCGMeshSamplingMethod::OnePointPerVertex"))
	bool bUseRedAsDensity = false;

	/** Enable voxelisation as a preparation pass. Can be more expensive given the VoxelSize. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Voxelize Options", meta = (PCG_Overridable))
	bool bVoxelize = false;

	/** Size of a voxel for the voxelization. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Voxelize Options", meta = (PCG_Overridable, EditCondition = "bVoxelize"))
	float VoxelSize = 100.0f;

	/** Post-processing pass after voxelization to remove hidden triangles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Voxelize Options", meta = (PCG_Overridable, EditCondition = "bVoxelize"))
	bool bRemoveHiddenTriangles = true;

	/** LOD type to use when creating DynamicMesh from specified StaticMesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|LODSettings", meta = (PCG_Overridable))
	EGeometryScriptLODType RequestedLODType = EGeometryScriptLODType::RenderData;

	UPROPERTY()
	int32 RequestedLODIndex = 0;

	// Poisson Sampling parameters
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Poisson sampling", meta = (PCG_Overridable, EditCondition = "SamplingMethod == EPCGMeshSamplingMethod::PoissonSampling"))
	FGeometryScriptMeshPointSamplingOptions SamplingOptions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Poisson sampling", meta = (PCG_Overridable, EditCondition = "SamplingMethod == EPCGMeshSamplingMethod::PoissonSampling"))
	FGeometryScriptNonUniformPointSamplingOptions NonUniformSamplingOptions;
};

/**
* Extra context to store all the data that need to be kept between multiple executions (time slicing)
*/
struct FPCGMeshSamplerContext : public FPCGContext
{
	~FPCGMeshSamplerContext();
	
	// Dynamic mesh. Will be added to root.
	TObjectPtr<UDynamicMesh> DynamicMesh;

	// Lists extracted from the mesh
	FGeometryScriptVectorList Positions;
	FGeometryScriptColorList Colors;
	FGeometryScriptVectorList Normals;
	FGeometryScriptIndexList TriangleIds;

	// Output point data
	UPCGPointData* OutPointData = nullptr;

	// For Poisson sampling, we are starting a future that is not framebound
	// Store the future and synchronisation items in the context
	TFuture<bool> SamplingFuture;
	std::atomic<bool> StopSampling = false;
	TUniquePtr<FProgressCancel> SamplingProgess;

	// Number of iterations to be done
	int32 Iterations = 0;

	// Set to true if prepared data succeeded.
	bool bDataPrepared = false;
};

class FPCGMeshSamplerElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
protected:
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};