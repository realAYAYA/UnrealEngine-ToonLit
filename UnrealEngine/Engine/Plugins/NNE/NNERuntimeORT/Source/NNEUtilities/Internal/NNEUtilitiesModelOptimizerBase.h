// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NNEModelOptimizerInterface.h"
#include "NNEUtilitiesModelOptimizer.h"
#include "Templates/SharedPointer.h"

struct FNNEModelRaw;

namespace UE::NNEUtilities::Internal
{
class FModelValidatorONNX : public NNE::Internal::IModelValidator
{
public:
	virtual FString GetName() const override;
	virtual bool ValidateModel(const FNNEModelRaw& InputModel, const FOptimizerOptionsMap& Options) const override;
};

class FModelOptimizerBase : public NNE::Internal::IModelOptimizer
{
protected:
	FModelOptimizerBase() {}
	TArray<TSharedPtr<NNE::Internal::IModelOptimizerPass>> OptimizationPasses;
	TArray<TSharedPtr<NNE::Internal::IModelValidator>> Validators;

	bool IsModelValid(const FNNEModelRaw& ModelToValidate, const FOptimizerOptionsMap& Options);
	bool ApplyAllPassesAndValidations(FNNEModelRaw& OptimizedModel, const FOptimizerOptionsMap& Options);

public:
	virtual void AddOptimizationPass(TSharedPtr<NNE::Internal::IModelOptimizerPass> ModelOptimizerPass) override;
	virtual void AddValidator(TSharedPtr<NNE::Internal::IModelValidator> ModelValidator) override;
	virtual bool Optimize(const FNNEModelRaw& InputModel, FNNEModelRaw& OptimizedModel, const FOptimizerOptionsMap& Options) override;
};

} // namespace UE::NNEUtilities::Internal
