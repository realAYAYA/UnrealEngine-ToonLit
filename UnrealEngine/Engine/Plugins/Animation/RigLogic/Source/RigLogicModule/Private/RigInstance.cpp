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

float FRigInstance::GetGUIControl(uint16 Index) const
{
	return RigInstance->getGUIControl(Index);
}

void FRigInstance::SetGUIControl(uint16 Index, float Value)
{
	RigInstance->setGUIControl(Index, Value);
}

TArrayView<const float> FRigInstance::GetGUIControlValues() const
{
	rl4::ConstArrayView<float> Values = RigInstance->getGUIControlValues();
	return TArrayView<const float>{Values.data(), static_cast<int32>(Values.size())};
}

void FRigInstance::SetGUIControlValues(const float* Values)
{
	RigInstance->setGUIControlValues(Values);
}

uint16 FRigInstance::GetRawControlCount() const
{
	return RigInstance->getRawControlCount();
}

float FRigInstance::GetRawControl(uint16 Index) const
{
	return RigInstance->getRawControl(Index);
}

void FRigInstance::SetRawControl(uint16 Index, float Value)
{
	RigInstance->setRawControl(Index, Value);
}

TArrayView<const float> FRigInstance::GetRawControlValues() const
{
	rl4::ConstArrayView<float> Values = RigInstance->getRawControlValues();
	return TArrayView<const float>{Values.data(), static_cast<int32>(Values.size())};
}

void FRigInstance::SetRawControlValues(const float* Values)
{
	RigInstance->setRawControlValues(Values);
}

uint16 FRigInstance::GetPSDControlCount() const
{
	return RigInstance->getPSDControlCount();
}

float FRigInstance::GetPSDControl(uint16 Index) const
{
	return RigInstance->getPSDControl(Index);
}

TArrayView<const float> FRigInstance::GetPSDControlValues() const
{
	rl4::ConstArrayView<float> Values = RigInstance->getPSDControlValues();
	return TArrayView<const float>{Values.data(), static_cast<int32>(Values.size())};
}

uint16 FRigInstance::GetMLControlCount() const
{
	return RigInstance->getMLControlCount();
}

float FRigInstance::GetMLControl(uint16 Index) const
{
	return RigInstance->getMLControl(Index);
}

TArrayView<const float> FRigInstance::GetMLControlValues() const
{
	rl4::ConstArrayView<float> Values = RigInstance->getMLControlValues();
	return TArrayView<const float>{Values.data(), static_cast<int32>(Values.size())};
}

uint16 FRigInstance::GetNeuralNetworkCount() const
{
	return RigInstance->getNeuralNetworkCount();
}

float FRigInstance::GetNeuralNetworkMask(uint16 NeuralNetIndex) const
{
	return RigInstance->getNeuralNetworkMask(NeuralNetIndex);
}

void FRigInstance::SetNeuralNetworkMask(uint16 NeuralNetIndex, float Value)
{
	RigInstance->setNeuralNetworkMask(NeuralNetIndex, Value);
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
