// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigLogic.h"

#include "DNAReader.h"
#include "FMemoryResource.h"
#include "RigInstance.h"

#include "riglogic/RigLogic.h"

void FRigLogic::FRigLogicDeleter::operator()(rl4::RigLogic* Pointer)
{
	rl4::RigLogic::destroy(Pointer);
}

FRigLogic::FRigLogic(const IBehaviorReader* Reader, ERigLogicCalculationType CalculationType) :
	MemoryResource{FMemoryResource::SharedInstance()},
	RigLogic{rl4::RigLogic::create(Reader->Unwrap(), rl4::CalculationType::SSE, FMemoryResource::Instance())}
{

}

FRigLogic::~FRigLogic() = default;

uint16 FRigLogic::GetLODCount() const
{
	return RigLogic->getLODCount();
}

TArrayView<const float> FRigLogic::GetRawNeutralJointValues() const
{
	rl4::ConstArrayView<float> Values = RigLogic->getRawNeutralJointValues();
	return TArrayView<const float>{Values.data(), static_cast<int32>(Values.size())};
}

FTransformArrayView FRigLogic::GetNeutralJointValues() const
{
	rl4::ConstArrayView<float> Values = RigLogic->getRawNeutralJointValues();
	return FTransformArrayView{Values.data(), Values.size()};
}

uint16 FRigLogic::GetJointGroupCount() const
{
	return RigLogic->getJointGroupCount();
}

TArrayView<const uint16> FRigLogic::GetJointVariableAttributeIndices(uint16 LOD) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getJointVariableAttributeIndices(LOD);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

void FRigLogic::MapGUIToRawControls(FRigInstance* Instance) const
{
	RigLogic->mapGUIToRawControls(Instance->Unwrap());
}

void FRigLogic::CalculateControls(FRigInstance* Instance) const
{
	RigLogic->calculateControls(Instance->Unwrap());
}

void FRigLogic::CalculateJoints(FRigInstance* Instance) const
{
	RigLogic->calculateJoints(Instance->Unwrap());
}

void FRigLogic::CalculateJoints(FRigInstance* Instance, uint16 JointGroupIndex) const
{
	RigLogic->calculateJoints(Instance->Unwrap(), JointGroupIndex);
}

void FRigLogic::CalculateBlendShapes(FRigInstance* Instance) const
{
	RigLogic->calculateBlendShapes(Instance->Unwrap());
}

void FRigLogic::CalculateAnimatedMaps(FRigInstance* Instance) const
{
	RigLogic->calculateAnimatedMaps(Instance->Unwrap());
}

void FRigLogic::Calculate(FRigInstance* Instance) const
{
	RigLogic->calculate(Instance->Unwrap());
}

rl4::RigLogic* FRigLogic::Unwrap() const
{
	return RigLogic.Get();
}
