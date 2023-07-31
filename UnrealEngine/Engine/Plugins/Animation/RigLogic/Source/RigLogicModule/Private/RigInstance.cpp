// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigInstance.h"

#include "FMemoryResource.h"
#include "RigLogic.h"

#include "riglogic/RigLogic.h"

void FRigInstance::FRigInstanceDeleter::operator()(rl4::RigInstance* Pointer)
{
	rl4::RigInstance::destroy(Pointer);
}

FRigInstance::FRigInstance(FRigLogic* RigLogic) :
	MemoryResource{FMemoryResource::SharedInstance()},
	RigInstance{rl4::RigInstance::create(RigLogic->Unwrap(), MemoryResource.Get())}
{
}

FRigInstance::~FRigInstance() = default;

uint16 FRigInstance::GetGUIControlCount() const
{
	return RigInstance->getGUIControlCount();
}

void FRigInstance::SetGUIControl(uint16 Index, float Value)
{
	RigInstance->setGUIControl(Index, Value);
}

void FRigInstance::SetGUIControlValues(const float* Values)
{
	RigInstance->setGUIControlValues(Values);
}

uint16 FRigInstance::GetRawControlCount() const
{
	return RigInstance->getRawControlCount();
}

void FRigInstance::SetRawControl(uint16 Index, float Value)
{
	RigInstance->setRawControl(Index, Value);
}

void FRigInstance::SetRawControlValues(const float* Values)
{
	RigInstance->setRawControlValues(Values);
}

TArrayView<const float> FRigInstance::GetRawJointOutputs() const
{
	rl4::ConstArrayView<float> Outputs = RigInstance->getRawJointOutputs();
	return TArrayView<const float>{Outputs.data(), static_cast<int32>(Outputs.size())};
}

FTransformArrayView FRigInstance::GetJointOutputs() const
{
	rl4::ConstArrayView<float> Outputs = RigInstance->getRawJointOutputs();
	return FTransformArrayView(Outputs.data(), Outputs.size());
}

TArrayView<const float> FRigInstance::GetBlendShapeOutputs() const
{
	rl4::ConstArrayView<float> Outputs = RigInstance->getBlendShapeOutputs();
	return TArrayView<const float>{Outputs.data(), static_cast<int32>(Outputs.size())};
}

TArrayView<const float> FRigInstance::GetAnimatedMapOutputs() const
{
	rl4::ConstArrayView<float> Outputs = RigInstance->getAnimatedMapOutputs();
	return TArrayView<const float>{Outputs.data(), static_cast<int32>(Outputs.size())};
}

uint16 FRigInstance::GetLOD() const
{
	return RigInstance->getLOD();
}

void FRigInstance::SetLOD(uint16 LOD)
{
	RigInstance->setLOD(LOD);
}

rl4::RigInstance* FRigInstance::Unwrap() const
{
	return RigInstance.Get();
}
