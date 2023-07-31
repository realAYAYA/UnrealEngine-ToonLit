// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshSpaceDeformerOp.h"

namespace UE
{
namespace Geometry
{

class MODELINGOPERATORS_API FBendMeshOp : public FMeshSpaceDeformerOp
{
public:
	virtual void CalculateResult(FProgressCancel* Progress) override;

	double BendDegrees = 90;
	bool bLockBottom = false;

protected:
};

} // end namespace UE::Geometry
} // end namespace UE
