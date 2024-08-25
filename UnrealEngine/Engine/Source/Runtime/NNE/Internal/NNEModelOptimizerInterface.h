// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointerFwd.h"

struct FNNEModelRaw;
namespace UE::NNE { class FAttributeMap; }

namespace UE::NNE::Internal
{
	
//Note: create a dedicated FOptimizerOptionsMap when function diverge with FAttributeMap
//example introduction of sparse tensor to FAttribute witch make no sense as an optimizer attribute
using FOptimizerOptionsMap = UE::NNE::FAttributeMap;

/** Interface class for NNE model validator */
class IModelValidator
{
public:
	virtual ~IModelValidator() = default;
	virtual FString GetName() const = 0;
	virtual bool ValidateModel(const FNNEModelRaw& InputModel, const FOptimizerOptionsMap& Options) const = 0;
};

/** Interface class for NNE model optimizer pass */
class IModelOptimizerPass
{
public:
	virtual ~IModelOptimizerPass() = default;
	virtual FString GetName() const = 0;

	//Optimize the model in place, potentially changing the format
	virtual bool ApplyPass(FNNEModelRaw& Model, const FOptimizerOptionsMap& Options) const = 0;
};

/** Interface class for NNE model optimizer */
class IModelOptimizer
{
public:
	virtual ~IModelOptimizer() = default;
	virtual FString GetName() const = 0;

	//Allow to extend/customize an optimizer by adding passes. They should be executed in order.
	virtual void AddOptimizationPass(TSharedPtr<IModelOptimizerPass> ModelOptimizerPass) = 0;
	
	//Allow to extend/customize an optimizer all validators should be run between each pass.
	virtual void AddValidator(TSharedPtr<IModelValidator>) = 0;
	
	//Apply all passes and validators to the input model, produce an optimized model potentially in a different format
	virtual bool Optimize(const FNNEModelRaw& InputModel, FNNEModelRaw& OutModel, const FOptimizerOptionsMap& Options) = 0;
};

} // namespace UE::NNE::Internal
