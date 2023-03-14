// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

#include "FMemoryResource.h"
#include "TransformArrayView.h"

class FRigInstance;
class IBehaviorReader;

namespace rl4
{

class RigLogic;

}  // namespace rl4

UENUM(BlueprintType)
enum class ERigLogicCalculationType: uint8
{
	Scalar,
	SSE,
	AVX
};

class RIGLOGICMODULE_API FRigLogic
{
public:
	FRigLogic(const IBehaviorReader* Reader, ERigLogicCalculationType CalculationType = ERigLogicCalculationType::SSE);
	~FRigLogic();

	FRigLogic(const FRigLogic&) = delete;
	FRigLogic& operator=(const FRigLogic&) = delete;

	FRigLogic(FRigLogic&&) = default;
	FRigLogic& operator=(FRigLogic&&) = default;

	uint16 GetLODCount() const;
	TArrayView<const float> GetRawNeutralJointValues() const;
	FTransformArrayView GetNeutralJointValues() const;
	uint16 GetJointGroupCount() const;
	TArrayView<const uint16> GetJointVariableAttributeIndices(uint16 LOD) const;
	void MapGUIToRawControls(FRigInstance* Instance) const;
	void CalculateControls(FRigInstance* Instance) const;
	void CalculateJoints(FRigInstance* Instance) const;
	void CalculateJoints(FRigInstance* Instance, uint16 JointGroupIndex) const;
	void CalculateBlendShapes(FRigInstance* Instance) const;
	void CalculateAnimatedMaps(FRigInstance* Instance) const;
	void Calculate(FRigInstance* Instance) const;

private:
	friend FRigInstance;
	rl4::RigLogic* Unwrap() const;

private:
	TSharedPtr<FMemoryResource> MemoryResource;

	struct FRigLogicDeleter
	{
		void operator()(rl4::RigLogic* Pointer);
	};
	TUniquePtr<rl4::RigLogic, FRigLogicDeleter> RigLogic;
};

