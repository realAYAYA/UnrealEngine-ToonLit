// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"
#include "DynamicMesh/MeshTangents.h"
#include "CalculateTangentsOp.generated.h"


UENUM()
enum class EMeshTangentsType : uint8
{
	/** Standard MikkTSpace tangent calculation */
	MikkTSpace = 0,
	/** MikkTSpace-like blended per-triangle tangents, with the blending being based on existing mesh, normals, and UV topology */
	FastMikkTSpace = 1,
	/** Project per-triangle tangents onto normals */
	PerTriangle = 2,
	/** Use existing source mesh tangents */
	CopyExisting = 3
};

namespace UE
{
namespace Geometry
{

class MODELINGOPERATORSEDITORONLY_API FCalculateTangentsOp : public TGenericDataOperator<FMeshTangentsd>
{
public:
	virtual ~FCalculateTangentsOp() {}

	// inputs
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> SourceMesh;
	TSharedPtr<FMeshTangentsf, ESPMode::ThreadSafe> SourceTangents;

	// parameters
	EMeshTangentsType CalculationMethod;

	// error flags
	bool bNoAttributesError = false;

	//
	// TGenericDataOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

	virtual void CalculateStandard(FProgressCancel* Progress, TUniquePtr<FMeshTangentsd>& Tangents);
	virtual void CalculateMikkT(FProgressCancel* Progress, TUniquePtr<FMeshTangentsd>& Tangents);
	virtual void CopyFromSource(FProgressCancel* Progress, TUniquePtr<FMeshTangentsd>& Tangents);
};

} // end namespace UE::Geometry
} // end namespace UE
