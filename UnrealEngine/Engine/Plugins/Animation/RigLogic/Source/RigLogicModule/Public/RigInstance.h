// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

#include "FMemoryResource.h"
#include "TransformArrayView.h"

class FRigLogic;

namespace rl4
{

class RigInstance;

}  // namespace rl4

class RIGLOGICMODULE_API FRigInstance
{
public:
	FRigInstance(FRigLogic* RigLogic);
	~FRigInstance();

	FRigInstance(const FRigInstance&) = delete;
	FRigInstance& operator=(const FRigInstance&) = delete;

	FRigInstance(FRigInstance&&) = default;
	FRigInstance& operator=(FRigInstance&&) = default;

	uint16 GetGUIControlCount() const;
	float GetGUIControl(uint16 Index) const;
	void SetGUIControl(uint16 Index, float Value);
	TArrayView<const float> GetGUIControlValues() const;
	void SetGUIControlValues(const float* Values);

	uint16 GetRawControlCount() const;
	float GetRawControl(uint16 Index) const;
	void SetRawControl(uint16 Index, float Value);
	TArrayView<const float> GetRawControlValues() const;
	void SetRawControlValues(const float* Values);

	uint16 GetPSDControlCount() const;
	float GetPSDControl(uint16 Index) const;
	TArrayView<const float> GetPSDControlValues() const;

	uint16 GetMLControlCount() const;
	float GetMLControl(uint16 Index) const;
	TArrayView<const float> GetMLControlValues() const;

	uint16 GetNeuralNetworkCount() const;
	float GetNeuralNetworkMask(uint16 NeuralNetIndex) const;
	void SetNeuralNetworkMask(uint16 NeuralNetIndex, float Value);

	TArrayView<const float> GetRawJointOutputs() const;
	FTransformArrayView GetJointOutputs() const;
	TArrayView<const float> GetBlendShapeOutputs() const;
	TArrayView<const float> GetAnimatedMapOutputs() const;
	uint16 GetLOD() const;
	void SetLOD(uint16 LOD);

private:
	friend FRigLogic;
	rl4::RigInstance* Unwrap() const;

private:
	TSharedPtr<FMemoryResource> MemoryResource;

	struct FRigInstanceDeleter
	{
		void operator()(rl4::RigInstance* Pointer);
	};
	TUniquePtr<rl4::RigInstance, FRigInstanceDeleter> RigInstance;
};
