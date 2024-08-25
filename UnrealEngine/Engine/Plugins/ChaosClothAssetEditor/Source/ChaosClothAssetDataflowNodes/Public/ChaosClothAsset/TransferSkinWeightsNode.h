// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "TransferSkinWeightsNode.generated.h"

class USkeletalMesh;

UENUM(BlueprintType)
enum class EChaosClothAssetTransferSkinWeightsMethod : uint8
{
	/** For every vertex on the target mesh, find the closest point on the surface of the source mesh and copy its weights. */
	ClosestPointOnSurface,
	
	/**
	 * For every vertex on the target mesh, find the closest point on the surface of the source mesh.
	 * If that point position is within the search radius, and their normals differ by less than the specified normal threshold,
	 * then the vertex weights are directly copied from the source point to the target mesh vertex.
	 * For all other vertices whose weights didn't get transferred, smoothed weight values are automatically computed.
	 */
	InpaintWeights
};


UENUM(BlueprintType)
enum class EChaosClothAssetMaxNumInfluences : uint8
{
	Uninitialized = 0	UMETA(Hidden),
	Four = 4			UMETA(DisplayName = "4"),
	Eight = 8			UMETA(DisplayName = "8"),
	Twelve = 12			UMETA(DisplayName = "12")
};

UENUM(BlueprintType)
enum class EChaosClothAssetTransferTargetMeshType : uint8
{
	/** Perform the skin weights transfer for both the simulation and render meshes. */
	All,

	/** Perform the skin weights transfer for the simulation mesh only. */
	Simulation,
	
	/** Perform the skin weights transfer for the render mesh only. */
	Render
};

UENUM(BlueprintType)
enum class EChaosClothAssetTransferRenderMeshSource : uint8
{
	/** For render mesh, transfer weights from the source skeletal mesh. */
	SkeletalMesh,

	/** For render mesh, transfer weights from the simulation mesh. */
	SimulationMesh
};

/** Transfer the skinning weights set on a skeletal mesh to the simulation and/or render mesh stored in the cloth collection. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetTransferSkinWeightsNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetTransferSkinWeightsNode, "TransferSkinWeights", "Cloth", "Cloth Transfer Skin Weights")

public:
	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The skeletal mesh to transfer the skin weights from. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Source Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** The skeletal mesh LOD to transfer the skin weights from. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Source Mesh", Meta = (DisplayName = "LOD Index"))
	int32 LodIndex = 0;

	/** The relative transform between the skeletal mesh and the cloth asset. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Source Mesh")
	FTransform Transform;

	/** The type of the mesh the transfer will be applied to. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (DisplayName = "Target Mesh Type"))
	EChaosClothAssetTransferTargetMeshType TargetMeshType = EChaosClothAssetTransferTargetMeshType::All;

	/** For the render mesh, choose which source to use. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (DisplayName = "Render Mesh Source Type", EditCondition="TargetMeshType!=EChaosClothAssetTransferTargetMeshType::Simulation"))
	EChaosClothAssetTransferRenderMeshSource RenderMeshSourceType = EChaosClothAssetTransferRenderMeshSource::SimulationMesh;

	/** Algorithm used for the transfer method. Use the simple ClosestPointOnSurface method or the more complex InpaintWeights method for better results. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (DisplayName = "Algorithm"))
	EChaosClothAssetTransferSkinWeightsMethod TransferMethod = EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights;

	/**
	 * Percentage of the bounding box diagonal of the simulation mesh to use as search radius for the InpaintWeights method.
	 * All points outside of the search radius will be ignored. 
	 * When set to a negative value (e.g. -1), all points will be considered.
	 */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (UIMin = -1, UIMax = 2, ClampMin = -1, ClampMax = 2, EditCondition="TransferMethod==EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights"))
	double RadiusPercentage = 0.05;

	/**
	 * Maximum angle difference (in degrees) between the target and source point normals to be considered a match for the InpaintWeights method.
	 * If set to a negative value (e.g. -1), normals will be ignored.
	 */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (UIMin = -1, UIMax = 180, ClampMin = -1, ClampMax = 180, EditCondition="TransferMethod==EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights"))
	double NormalThreshold = 30;

	/** 
	 * If true, when the closest point doesn't pass the normal threshold test, will try again with a flipped normal. 
	 * This helps with layered meshes where the "inner" and "outer" layers are close to each other but whose normals 
	 * are pointing in the opposite directions.
	 */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (DisplayName = "Layered Mesh Support", EditCondition="TransferMethod==EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights"))
	bool LayeredMeshSupport = true;

	/** The number of smoothing iterations applied to the vertices whose weights were automatically computed. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (UIMin = 0, UIMax = 100, ClampMin = 0, ClampMax = 100, DisplayName = "Smoothing Iterations", EditCondition="TransferMethod==EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights"))
	int32 NumSmoothingIterations = 10;

	/** The smoothing strength of each smoothing iteration. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (UIMin = 0, UIMax = 1, ClampMin = 0, ClampMax = 1, DisplayName = "Smoothing Strength", EditCondition="TransferMethod==EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights"))
	float SmoothingStrength = 0.1;
	
	/** The maximum number of bones that will influence each vertex. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (DisplayName = "Max Bone Influences"))
	EChaosClothAssetMaxNumInfluences MaxNumInfluences = EChaosClothAssetMaxNumInfluences::Eight;
	
    /** Optional mask where a non-zero value indicates that we want the skinning weights for the vertex to be computed automatically instead of it being copied over from the source mesh. */
	UPROPERTY(EditAnywhere, Category = "Transfer Skin Weights|Transfer Method", Meta = (DisplayName = "Inpaint Mask", EditCondition="TransferMethod==EChaosClothAssetTransferSkinWeightsMethod::InpaintWeights"))
	FChaosClothAssetWeightedValueNonAnimatableNoLowHighRange InpaintMask = { TEXT("InpaintMask") };

	FChaosClothAssetTransferSkinWeightsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
