// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNERuntimeRDG.h"

namespace UE::NNEDenoiser::Private
{

class IModelInstance : public NNE::IModelInstanceRDG
{
public:
	using ESetInputTensorShapesStatus = NNE::IModelInstanceRDG::ESetInputTensorShapesStatus;
	using EEnqueueRDGStatus = NNE::IModelInstanceRDG::EEnqueueRDGStatus;

	virtual ~IModelInstance() = default;
};

}