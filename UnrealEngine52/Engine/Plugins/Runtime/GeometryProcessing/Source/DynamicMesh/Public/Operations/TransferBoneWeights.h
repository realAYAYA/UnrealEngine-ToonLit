// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryTypes.h"
#include "UObject/NameTypes.h"


// Forward declarations
class FProgressCancel;

namespace UE::AnimationCore { class FBoneWeights; }

namespace UE::Geometry
{
	template<typename RealType> class TTransformSRT3;
	typedef TTransformSRT3<double> FTransformSRT3d;

	class FDynamicMesh3;
	template<typename MeshType> class TMeshAABBTree3;
	typedef TMeshAABBTree3<FDynamicMesh3> FDynamicMeshAABBTree3;
}


namespace UE
{
namespace Geometry
{
/**
 * Transfer bone weights from one mesh (source) to another (target). 
 * 
 * 
 * Example usage:
 * 
 * FDynamicMesh SourceMesh = ...; // Mesh we transfering weights from
 * FDynamicMesh TargetMesh = ...; // Mesh we are transfering weights into
 * 
 * // These can be initialized by accessing reference skeleton of the corresponding USkinnedAsset
 * TMap<uint16, FName> SourceIndexToBone = ...; // Map the bone index to the bone name for the source mesh
 * TMap<FName, uint16> TargetBoneToIndex = ...; // Map the bone name to the bone index for the target mesh
 *
 * FTransferBoneWeights TransferBoneWeights(&SourceMesh, FSkeletalMeshAttributes::DefaultSkinWeightProfileName);
 * TransferBoneWeights.SourceIndexToBone = &SourceIndexToBone; 
 * TransferBoneWeights.TargetBoneToIndex = &TargetBoneToIndex;
 * 
 * // Optionally transform the target mesh. This is useful when you want to align two meshes in world space.
 * FTransformSRT3d InToWorld = ...; 
 * 
 * if (TransferBoneWeights.Validate() == EOperationValidationResult::Ok) 
 * {
 *      TransferBoneWeights.Compute(TargetMesh, InToWorld, FSkeletalMeshAttributes::DefaultSkinWeightProfileName);
 * }
 */

class DYNAMICMESH_API FTransferBoneWeights
{
public:

	enum class ETransferBoneWeightsMethod : uint8
	{
        // For every vertex on the target mesh, find the closest point on the surface of the source mesh.
        // This is usually a point on a triangle where the bone weights are interpolated via barycentric coordinates.
		ClosestPointOnSurface = 0
	};

	//
	// Optional Inputs
	//
	
    /** Set this to be able to cancel the running operation. */
	FProgressCancel* Progress = nullptr;

	/** Enable/disable multi-threading. */
	bool bUseParallel = true;

    /** 
     * We assume that each mesh inherits its reference skeleton from the same USkeleton asset. However, their internal 
     * indexing can be different and hence when transfering weights we need to make sure we reference the correct bones 
     * via their names instead of indices.
     * 
     * TODO: We can store this information inside DynamicVertexSkinWeightsAttribute when converting Skeletal Meshes to Dynamic Meshes.
     */
	const TMap<uint16, FName>* SourceIndexToBone = nullptr; // Map the bone index to the bone name for the source mesh
	const TMap<FName, uint16>* TargetBoneToIndex = nullptr; // Map the bone name to the bone index for the target mesh
	
	/** The transfer method to compute the bone weights. */
	ETransferBoneWeightsMethod TransferMethod = ETransferBoneWeightsMethod::ClosestPointOnSurface;

protected:
		
	/** Source mesh we are transfering weights from. */
	const FDynamicMesh3* SourceMesh;
	
	/** The name of the source mesh skinning profile name. */
	FName SourceProfileName;

	/** 
	 * The caller can optionally specify the source mesh BVH in case this operator is run on multiple target meshes 
	 * while the source mesh remains the same. Otherwise BVH tree will be computed.
	 */
	const FDynamicMeshAABBTree3* SourceBVH = nullptr;

	/** If the caller doesn't pass BVH for the source mesh then we compute one. */
	TUniquePtr<FDynamicMeshAABBTree3> InternalSourceBVH;

public:

	FTransferBoneWeights(const FDynamicMesh3* InSourceMesh, 
					     const FName& InSourceProfileName,
					     const FDynamicMeshAABBTree3* SourceBVH = nullptr); 
	
	virtual ~FTransferBoneWeights();

	/**
	 * @return EOperationValidationResult::Ok if we can apply operation, or error code if we cannot.
	 */
	virtual EOperationValidationResult Validate();

	/**
     * Transfer the bone weights from the source mesh to the given target mesh and store the result in the skin weight  
     * attribute with the given profile name.
	 * 
	 * @param InOutTargetMesh Target mesh we are transfering weights into
	 * @param InToWorld Transform applied to the input target mesh
     * @param InTargetProfileName Skin weight profile name we are writing into. If the profile with that name exists,  
     *       					  then the data will be overwritten, otherwise a new attribute will be created.
     * 
     * @return true if the algorithm succeeds, false if it failed or was canceled by the user.
	 */
	virtual bool Compute(FDynamicMesh3& InOutTargetMesh, const FTransformSRT3d& InToWorld, const FName& InTargetProfileName);

	/**
     * Compute a single bone weight for a given point.
     *
	 * @param InPoint Point for which we are computing a bone weight
	 * @param InToWorld Transform applied to the point
     * @param OutWeights Bone weight computed for the input transformed point
	 * 
     * @return true if the algorithm succeeds, false if it failed or was canceled by the user.
	 */
	virtual bool Compute(const FVector3d& InPoint, const FTransformSRT3d& InToWorld, UE::AnimationCore::FBoneWeights& OutWeights);

protected:
	
    /** @return if true, abort the computation. */
	virtual bool Cancelled();
};

} // end namespace UE::Geometry
} // end namespace UE
