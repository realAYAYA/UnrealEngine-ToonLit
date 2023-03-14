// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

enum class EMeshAttributeTransferType
{
	MaterialID
};

enum class EMeshAttributeTransferError
{
	InvalidSource,
	InvalidTarget,
	SourceMissingAttribute,
	TargetMissingAttribute,
	InvalidOperationForInputs
};

/**
 * FMeshAttributeTransfer transfers attributes from a SourceMesh to a TargetMesh
 */
class DYNAMICMESH_API FMeshAttributeTransfer
{
public:
	/** The mesh that we are transferring from */
	const FDynamicMesh3* SourceMesh;

	/** The mesh that we are transferring to */
	FDynamicMesh3* TargetMesh;

	/** What is being transferred */
	EMeshAttributeTransferType TransferType = EMeshAttributeTransferType::MaterialID;


	TArray<EMeshAttributeTransferError> Errors;

public:
	FMeshAttributeTransfer(const FDynamicMesh3* SourceMeshIn, FDynamicMesh3* TargetMeshIn);

	virtual ~FMeshAttributeTransfer() {}

	/**
	 * Run the operation and modify .Mesh
	 * @return true if the algorithm succeeds
	 */
	virtual bool Apply();

protected:

	TUniquePtr<FDynamicMeshAABBTree3> SourceSpatial;


	bool Apply_MaterialID();
};

} // end namespace UE::Geometry
} // end namespace UE